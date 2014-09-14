#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <sstream>
//#include "mpi.h"

using std::cout;
using std::cin;
using std::endl;
//using namespace MPI;

// Prototype definitions
inline int getIndex(int, int, int);
void initGrid(int, int, bool[]);
int getNeighbors(int, int, int, bool[]);
void step(int, bool[], bool[]);
void doHallo(int, bool[]);
void copyGrid(int, bool[], bool[]);
void makeImage(int, int, bool[]);
// end of prototype definitions


/**
 * Determines the index in a one-dimensional interpretation
 * of the (gridSize + 2)x(gridSize + 2) matrix
 */
inline int getIndex(int gridSize, int y, int x)
{
	return x + (gridSize + 2) * y;
}

/**
 * fills every cell of the grid with a predefined probability
 * fills the hallo too
 */
void initGrid(int gridSize, int probability, bool grid[])
{
	//fill with probability
	int y, x;
	for(y = 1; y <= gridSize; y++)
	{
		for(x = 1; x <= gridSize; x++)
		{
			int random =  (100.0 * rand()) / RAND_MAX;
			if(random < probability)
				grid[getIndex(gridSize, y, x)] = true;
			else
				grid[getIndex(gridSize, y, x)] = false;
		}
	}

	doHallo(gridSize, grid);
}


/**
 * Returns the neighbors count for this cell. x,y are between 1 and gridSize
 */
int getNeighbors(int gridSize, int y, int x, bool grid[])
{
	int count = 0;
	if(grid[getIndex(gridSize, y - 1, x - 1)])
		count++;
	if(grid[getIndex(gridSize, y - 1, x)])
		count++;
	if(grid[getIndex(gridSize, y - 1, x + 1)])
		count++;
	if(grid[getIndex(gridSize, y, x - 1)])
		count++;
	if(grid[getIndex(gridSize, y, x + 1)])
		count++;
	if(grid[getIndex(gridSize, y + 1, x - 1)])
		count++;
	if(grid[getIndex(gridSize, y + 1, x)])
		count++;
	if(grid[getIndex(gridSize, y + 1, x + 1)])
		count++;
	return count;
}

/**
 * Applies the rules to the grid of cells:
 * Every dead cell with exactly 3 neighbors will come to live
 * Every living cell with more than 3 or less than 2 neighbors will die
 */
void step(int gridSize, bool grid[], bool nextGrid[])
{
	int x, y;
	for(y = 1; y < gridSize; y++)
	{
		for(x = 1; x < gridSize; x++)
		{
			int index = getIndex(gridSize, y, x);
			int neighbors = getNeighbors(gridSize, y, x, grid);
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
 * Recalculates the hallo
 */
void doHallo(int gridSize, bool grid[])
{
	int i;
	//horizontal hallo and vertical
	for(i = 1; i <= gridSize; i++)
	{
		grid[getIndex(gridSize, 0, i)] = grid[getIndex(gridSize, gridSize, i)];
		grid[getIndex(gridSize, gridSize + 1, i)] = grid[getIndex(gridSize, 1, i)];
		grid[getIndex(gridSize, i, 0)] = grid[getIndex(gridSize, i, gridSize)];
		grid[getIndex(gridSize, i, gridSize + 1)] = grid[getIndex(gridSize, i, 1)];
	}

	//hallo corners
	grid[getIndex(gridSize, 0, 0)] = grid[getIndex(gridSize, gridSize, gridSize)];
	grid[getIndex(gridSize, 0, gridSize + 1)] = grid[getIndex(gridSize, gridSize, 1)];
	grid[getIndex(gridSize, gridSize + 1, 0)] = grid[getIndex(gridSize, 1, gridSize)];
	grid[getIndex(gridSize, gridSize + 1, gridSize + 1)] = grid[getIndex(gridSize, 1, 1)];
}

/**
 * Copies the contents of source to target. Will copy hallo as well
 */
void copyGrid(int gridSize, bool source[], bool target[])
{
	for(int i = 0; i < (gridSize + 2) * (gridSize + 2); i++)
		target[i] = source[i];
}

void makeImage(int gridSize, int step, bool grid[])
{
	const int squareSize = 5;
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

int main(int argv, char* argc[])
{
	// parameters input
	int gridSize;
	int probability;
	int steps;
	cout << "Enter grid size: ";
	cin >> gridSize;
	cout << "Enter initial fill probability [0 - 100]: ";
	cin >> probability;
	cout << "Enter steps count: ";
	cin >> steps;

	// initialization of the grid matrix for the current step and the next step
	bool* grid =     new bool[(gridSize + 2) * (gridSize + 2)];
	bool* nextGrid = new bool[(gridSize + 2) * (gridSize + 2)];
	initGrid(gridSize, probability, grid);
	makeImage(gridSize, 0, grid);

	for(int i = 1; i <= steps; i++)
	{
		step(gridSize, grid, nextGrid);
		makeImage(gridSize, i, nextGrid);
		doHallo(gridSize, nextGrid);
		copyGrid(gridSize, nextGrid, grid);
	}

	return 0;
}
