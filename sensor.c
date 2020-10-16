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

    while(currentIteration < maxIterations){
        // Initialize the pack buffer with zeros
        int position = 0;

        // Generate temperature and send to station
        int temperature = randomValue(TEMP_LOW, TEMP_HIGH, rank);
        char alertTime[datesize];
        char nodeIPMAC[2][20];
        strncpy(nodeIPMAC[0], &address, 20);
        strncpy(nodeIPMAC[1], &MAC, 20);

        getTimeStamp(alertTime);
        int neighborDetails[4][3];
        char neighborIP[4][20];
        char neighborMAC[4][20];

        for(int i=0 ; i<4 ; i++){
            int neighborCoord[2];
            neighborDetails[i][0] = neighborDetails[i][1] = neighborDetails[i][2] = -1;
            strncpy(neighborIP[i], "-1", 20);
            strncpy(neighborMAC[i], "-1", 20);
            if(neighbors[i] != -2){
                MPI_Cart_coords(grid_comm, neighbors[i], ndims, neighborCoord);
                neighborDetails[i][0] = neighborCoord[0];
                neighborDetails[i][1] = neighborCoord[1];
                neighborDetails[i][2] = 100;
                strncpy(neighborIP[i], &address, 20);
                strncpy(neighborMAC[i], &MAC, 20);
            }
        }

        // Get the event time to calculate communication time
        double eventStartTime = MPI_Wtime();

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
        sleep(iterationSleep);
    }


    // If temperature exceeds 80 degrees, send a message to all non-null adjacent neighbors
    // Neighbors will send back their last recorded temperature
    // if(temperature > TEMP_THRESHOLD){
    //     for(int i=0 ; i<4 ; i++){
    //         MPI_Send(&rank, 1, MPI_INT, neighbors[i], REQUEST_TAG, grid_comm);
    //     }
    // }
    // // Else listen to any requests and send back temperature if asked
    // else{
    //     int neighbor_rank = 0;
    //     MPI_Status  receive_status[4];
    //     for(int i=0 ; i<4 ; i++){
    //         MPI_Recv(&neighbor_rank, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_TAG, grid_comm);
    //         MPI_Send(&temperature, 1, MPI_INT, neighbor_rank, REPLY_TAG, grid_comm, receive_status[i]);
    //     }
    // }
    MPI_Comm_free(&grid_comm);
	return 0;
}
