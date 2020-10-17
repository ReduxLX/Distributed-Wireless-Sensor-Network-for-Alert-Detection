/* 
FIT 3143 Assignment 2
Authors [A~Z]:
    Alfons Fernaldy 30127831
    Matthew Khoo 29270294
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <mpi.h>

#include "./main.h"
#include "./sensor.h"

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

/* Import Global Variables */
extern int    stationRank;
extern int    row, column;
extern int    maxIterations;
extern int    buffsize;
extern int    datesize;
extern int    userstop;

extern int    TEMP_LOW;
extern int    TEMP_HIGH;
extern int    TEMP_THRESHOLD;

extern double iterationSleep;
extern int    cummulativeSeed;
extern char   address;
extern char   MAC;

int slave(MPI_Comm station_comm, int rank, int size){
    sleep(1);
    MPI_Comm grid_comm;
    int  ndims = 2;
    int  reorder = 1;
    int  dims[2] = {row, column};
    int  period[2] = {0, 0};
    int  coord[2];
    int  neighbors[4];
    int  nrows, ncols;
    char packbuf[buffsize];
    char packbuf2[buffsize];
    // MPI_Dims_create finds the best dimension for the cartesian grid 
    MPI_Dims_create(size-1, ndims, dims);

    // MPI_Cart_create creates a 1D, 2D or 3D virtual topology in a cartesian grid
    int ierr = 0;
    ierr = MPI_Cart_create(station_comm, ndims, dims, period, reorder, &grid_comm);
    if(ierr != 0) printf("ERROR[%d] creating CART\n",ierr);

    // MPI_Cart_coords finds my coordinates in the cartesian communicator group
    MPI_Cart_coords(grid_comm, rank, ndims, coord);
    // MPI_Cart_rank(comm2D, coord, &my_cart_rank);

    // MPI_Cart_shift is used to find adjacent node's ranks and store them in neighbors array
    MPI_Cart_shift(grid_comm, SHIFT_ROW, DISP, &neighbors[0], &neighbors[1]);
    MPI_Cart_shift(grid_comm, SHIFT_COL, DISP, &neighbors[2], &neighbors[3]);

    int currentIteration = 0;

    // Get the non blocking recieve from the POSIX thread
    MPI_Request temp_req;
	MPI_Irecv(&userstop, 1, MPI_INT, stationRank, 3, MPI_COMM_WORLD, &temp_req);

    while(maxIterations == -1 || currentIteration < maxIterations){
        printf("SENSOR %d user stop %d\n", rank, userstop);
        // Initialize the pack buffer with zeros
        int position = 0;

        // Generate temperature and send to station
        int temperature = randomValue(TEMP_LOW, TEMP_HIGH, rank);

        // Get neighbor's temperature, IP and MAC addresses
        MPI_Status status[3];
        int  neighborTemp[4] = {-1, -1, -1, -1};
        char neighborIP[4][20];
        char neighborMAC[4][20];
        for(int i=0 ; i<4 ; i++){
            MPI_Send(&temperature, 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD);
            MPI_Send(&address, 20, MPI_CHAR, neighbors[i], 0, MPI_COMM_WORLD);
            MPI_Send(&MAC, 20, MPI_CHAR, neighbors[i], 0, MPI_COMM_WORLD);
            MPI_Recv(&neighborTemp[i], 1, MPI_INT, neighbors[i], 0, MPI_COMM_WORLD, &status[0]);
            MPI_Recv(&neighborIP[i], 20, MPI_CHAR, neighbors[i], 0, MPI_COMM_WORLD, &status[1]);
            MPI_Recv(&neighborMAC[i], 20, MPI_CHAR, neighbors[i], 0, MPI_COMM_WORLD, &status[2]);
        }

        char alertTime[datesize];
        char nodeIPMAC[2][20];
        strncpy(nodeIPMAC[0], &address, 20);
        strncpy(nodeIPMAC[1], &MAC, 20);

        getTimeStamp(alertTime);
        int  neighborDetails[4][3];

        for(int i=0 ; i<4 ; i++){
            int neighborCoord[2];
            neighborDetails[i][0] = neighborDetails[i][1] = neighborDetails[i][2] = -1;
            if(neighbors[i] != -2){
                MPI_Cart_coords(grid_comm, neighbors[i], ndims, neighborCoord);
                neighborDetails[i][0] = neighborCoord[0];
                neighborDetails[i][1] = neighborCoord[1];
                neighborDetails[i][2] = neighborTemp[i];
            }
        }

        // Get the event time to calculate communication time
        double eventStartTime = MPI_Wtime();
        position = 0;

        // Pack all the info needed to be sent to the base station
        MPI_Pack(&eventStartTime, 1, MPI_DOUBLE, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&temperature, 1, MPI_INT, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&alertTime, datesize, MPI_CHAR, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&coord, 2, MPI_INT, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&nodeIPMAC, 40, MPI_CHAR, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&neighborDetails, 12, MPI_INT, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&neighborIP, 80, MPI_CHAR, packbuf, buffsize, &position, MPI_COMM_WORLD);
        MPI_Pack(&neighborMAC, 80, MPI_CHAR, packbuf, buffsize, &position, MPI_COMM_WORLD);

        MPI_Send(packbuf, buffsize, MPI_PACKED, stationRank, 0, MPI_COMM_WORLD);

        currentIteration++;
        cummulativeSeed = 1;
        if (userstop == 1){
			break;
		}
        sleep(iterationSleep);
    }


    MPI_Comm_free(&grid_comm);
	return 0;
}
