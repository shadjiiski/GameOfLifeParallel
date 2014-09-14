#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include "mpi.h"

using std::cout;
using std::cin;
using std::endl;
using namespace MPI;

const int TAG_HALO_SWAP = 1;
const int squareSize = 2;

// hardcoded values to use when input is disabled. E.g. running a batch job on a cluster
const bool noinput = false;
const int noInputSize = 1000;
const int noInputProbability = 20;
const int noInputSteps = 100;

// Prototype definitions
inline int getIndex(int, int, int);
void initGrid(int, int, int, bool[]);
int getNeighbors(int, int, int, bool[]);
void step(int, int, bool[], bool[]);
void swapHalo(Cartcomm, Datatype, int, int, bool[]);
void copyGrid(int, int, bool[], bool[]);
void makeImage(int, int, bool[]);
// end of prototype definitions


/**
 * Determines the index in a one-dimensional interpretation
 * of the (height + 2)x(width + 2) matrix - fraction of the whole grid
 * with halo border added
 */
inline int getIndex(int width, int y, int x)
{
	return x + (width + 2) * y;
}

/**
 * fills every cell of the grid with a predefined probability
 */
void initGrid(int height, int width, int probability, bool grid[])
{
	//fill with probability
	int y, x;
	for(y = 1; y <= height; y++)
	{
		for(x = 1; x <= width; x++)
		{
			int random =  (100.0 * rand()) / RAND_MAX;
			if(random < probability)
				grid[getIndex(width, y, x)] = true;
			else
				grid[getIndex(width, y, x)] = false;
		}
	}
}


/**
 * Returns the neighbors count for this cell. x,y are between 1 and gridSize
 */
int getNeighbors(int width, int y, int x, bool grid[])
{
	int count = 0;
	if(grid[getIndex(width, y - 1, x - 1)])
		count++;
	if(grid[getIndex(width, y - 1, x)])
		count++;
	if(grid[getIndex(width, y - 1, x + 1)])
		count++;
	if(grid[getIndex(width, y, x - 1)])
		count++;
	if(grid[getIndex(width, y, x + 1)])
		count++;
	if(grid[getIndex(width, y + 1, x - 1)])
		count++;
	if(grid[getIndex(width, y + 1, x)])
		count++;
	if(grid[getIndex(width, y + 1, x + 1)])
		count++;
	return count;
}

/**
 * Applies the rules to the grid of cells:
 * Every dead cell with exactly 3 neighbors will come to live
 * Every living cell with more than 3 or less than 2 neighbors will die
 */
void step(int height, int width, bool grid[], bool nextGrid[])
{
	int x, y;
	for(y = 1; y <= height; y++)
	{
		for(x = 1; x <= width; x++)
		{
			int index = getIndex(width, y, x);
			int neighbors = getNeighbors(width, y, x, grid);
			//every dead cell with exactly 3 neighbors comes back to live
			if(!grid[index])
			{
				if(neighbors == 3)
					nextGrid[index] = true;
			}
			//every living cell with less than 2 or more than 3 neighbors dies
			else if(neighbors < 2 || neighbors > 3)
				nextGrid[index] = false;
		}
	}
}

/**
 * Recalculates the halo
 */
void swapHalo(Cartcomm cartesian, Datatype rowType, int height, int width, bool grid[])
{
	int top, bottom;
	cartesian.Shift(0, 1, top, bottom);

	//send top halo
	grid[getIndex(width, 1, 0)] = grid[getIndex(width, height, width)];
	grid[getIndex(width, 1, width + 1)] = grid[getIndex(width, height, 1)];
	cartesian.Sendrecv(&grid[getIndex(width, 1, 0)], 1, rowType, top, TAG_HALO_SWAP, &grid[getIndex(width, height + 1, 0)], 1, rowType, bottom, TAG_HALO_SWAP);

	//send bottom halo
	grid[getIndex(width, height, 0)] = grid[getIndex(width, 1, width)];
	grid[getIndex(width, height, width + 1)] = grid[getIndex(width, 1, 1)];
	cartesian.Sendrecv(&grid[getIndex(width, height, 0)], 1, rowType, bottom, TAG_HALO_SWAP, grid, 1, rowType, top, TAG_HALO_SWAP);

	//now do the sides
	for(int y = 1; y <= height; y++)
	{
		grid[getIndex(width, y, 0)] = grid[getIndex(width, y, width)];
		grid[getIndex(width, y, width + 1)] = grid[getIndex(width, y, 1)];
	}
}

/**
 * Copies the contents of source to target. Will copy halo as well
 */
void copyGrid(int height, int width, bool source[], bool target[])
{
	for(int i = 0; i < (width + 2) * (height + 2); i++)
		target[i] = source[i];
}

void makeImage(int gridSize, int step, bool grid[])
{
	std::ostringstream sstream;
	sstream << "images/step" << step << ".pgm";
	std::ofstream stream(sstream.str().c_str());
	stream << "P2\n#Step " << step << " of the Game of Life.\n";
	stream << (gridSize * squareSize) << " " << (gridSize * squareSize) << "\n" << 1 << "\n";

	int symbols = 0;
	int y, x, repy, repx;
	for(y = 1; y <= gridSize; y++)
	{
		//repeat every row squareSize times
		for(repy = 0; repy < squareSize; repy++)
		{
			for(x = 1; x <= gridSize; x++)
			{
				//repeat every column squareSize times
				for(repx = 0; repx < squareSize; repx++)
				{
					//living cells are black
					if(grid[getIndex(gridSize, y, x)])
						stream << "0";
					//dead cells are white
					else
						stream << "1";
					if((++symbols) % 30 == 0) // no more than 30 pixels per row (60 chars)
						stream << "\n";
					else
						stream << " ";
				}
			}
		}
	}
}

int main(int argc, char* argv[])
{
	// MPI initialization and Cartesian topology creation
	Init(argc, argv);
	int cpus = COMM_WORLD.Get_size();
	int rank = COMM_WORLD.Get_rank();
	srand(time(NULL) + rank);
	int dims[1];
	dims[0] = cpus;
	bool periods[] = {true};
	Cartcomm cartesian = COMM_WORLD.Create_cart(1, dims, periods, false);

	// parameters input
	int gridSize;
	int probability;
	int steps;
	if(noinput)
	{
		gridSize = noInputSize;
		probability = noInputProbability;
		steps = noInputSteps;
	}
	else if(rank == 0)
	{
		cout << "Enter grid size: ";
		cin >> gridSize;
		cout << "Enter initial fill probability [0 - 100]: ";
		cin >> probability;
		cout << "Enter steps count: ";
		cin >> steps;
	}
	//Broadcast common parameters
	COMM_WORLD.Bcast(&gridSize,    1, INT, 0);
	COMM_WORLD.Bcast(&probability, 1, INT, 0);
	COMM_WORLD.Bcast(&steps,       1, INT, 0);

	int width  = gridSize;
	// accounts for non integral division of gridSize and cpus
	int height = rank < (gridSize % cpus) ? gridSize / cpus + 1 : gridSize / cpus;

	// initialization of the grid matrix for the current step and the next step.
	bool* wholeGrid = new bool[(gridSize + 2) * (gridSize + 2)];
	bool* grid      = new bool[(width + 2) * (height + 2)];
	bool* nextGrid  = new bool[(width + 2) * (height + 2)];
	Datatype rowType = BOOL.Create_contiguous((width + 2));
	rowType.Commit();
	for(int i = 0; i < gridSize * gridSize; i++)
	{
		if(i < (width + 2) * (height + 2))
		{
			grid[i] = false;
			nextGrid[i] = false;
		}
		wholeGrid[i] = false;
	}
	initGrid(height, width, probability, grid);
	swapHalo(cartesian, rowType, height, width, grid);
	//dump the zero-step image
	cartesian.Gather(grid + width + 2, height, rowType, wholeGrid + width + 2, height, rowType, 0);
	if(!noinput && rank == 0)
		makeImage(gridSize, 0, wholeGrid);
	double time = Wtime();

	for(int i = 1; i <= steps; i++)
	{
		step(height, width, grid, nextGrid);
		swapHalo(cartesian, rowType, height, width, nextGrid);
		copyGrid(height, width, nextGrid, grid);
		cartesian.Gather(grid + width + 2, height, rowType, wholeGrid + width + 2, height, rowType, 0);
		if(!noinput && rank == 0)
			makeImage(gridSize, i, wholeGrid);
	}

	if(rank == 0)
	{
		time = Wtime() - time;
		cout << "==== Execution completed ====" << endl;
		cout << "Grid size is " << gridSize << "x" << gridSize << endl;
		cout << "Initial population probability is " << probability << "%" << endl;
		cout << steps << " steps were made" << endl;
		if(noinput)
			cout << "No images were output" << endl;
		else
			cout << "Images are saved under the images/ directory" << endl;
		cout << "Execution took " << time << " seconds" << endl;
		cout << cpus << " processes were used" << endl;
	}

	rowType.Free();
	delete nextGrid;
	delete grid;
	delete wholeGrid;
	Finalize();
	return 0;
}
