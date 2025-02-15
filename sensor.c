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
#include <pthread.h>
#include <mpi.h>

#include "./main.h"
#include "./sensor.h"

/* Import Global Variables */
extern int    stationRank;
extern int    row, column;
extern int    rank, size;
extern int    maxIterations;
extern int    stopSignal;
extern char   IP_address;
extern char   MAC;

int slave(MPI_Comm station_comm){
    sleep(1); // Give the station time to start satellite thread
    MPI_Comm grid_comm;
    int  ndims = 2;
    int  reorder = 1;
    int  dims[2] = {row, column};
    int  period[2] = {0, 0};
    int  coord[2];
    int  neighbors[4];
    char packbuf[packSize];
    char packbuf2[packSize];

    // MPI_Dims_create finds the best dimension for the cartesian grid 
    MPI_Dims_create(size-1, ndims, dims);

    // MPI_Cart_create creates a 1D, 2D or 3D virtual topology in a cartesian grid
    int ierr = 0;
    ierr = MPI_Cart_create(station_comm, ndims, dims, period, reorder, &grid_comm);
    if(ierr != 0) printf("ERROR[%d] creating CART\n",ierr);

    // MPI_Cart_coords finds my coordinates in the cartesian communicator group
    MPI_Cart_coords(grid_comm, rank, ndims, coord);

    // MPI_Cart_shift is used to find adjacent node's ranks and store them in neighbors array
    MPI_Cart_shift(grid_comm, SHIFT_ROW, DISP, &neighbors[0], &neighbors[1]);
    MPI_Cart_shift(grid_comm, SHIFT_COL, DISP, &neighbors[2], &neighbors[3]);

    // Initialize the current iteration
    int currentIteration = 1;

    // Start a non-blocking receive request to listen for any termination signal from base station
    MPI_Request receive_status;
	MPI_Irecv(&stopSignal, 1, MPI_INT, stationRank, 3, MPI_COMM_WORLD, &receive_status);

    // Keep iterating till currentIteration reaches maxIterations OR stop signal is received
    while(maxIterations == -1 || currentIteration <= maxIterations){
        // Initialize position pointer to 0
        int position = 0;
        // Generate a random temperature between TEMP_LOW - TEMP_HIGH
        int temperature = randomValue(TEMP_LOW, TEMP_HIGH, rank);

        // Initialize arrays to store neighboring node's temperature, IP and MAC addresses
        MPI_Status status[3];
        int  neighborTemp[4] = {-1, -1, -1, -1};
        char neighborIP[4][20];
        char neighborMAC[4][20];

        if(stopSignal == 1)
            printf("Rank %d received exit signal at iteration %d\n", rank, currentIteration);
        
        // Store current node's IP and MAC address in an array
        char nodeIPMAC[2][20];
        strncpy(nodeIPMAC[0], &IP_address, 20);
        strncpy(nodeIPMAC[1], &MAC, 20);

        // Exchange Temperature, IP and MAC addresses
        for(int i=0 ; i<4 ; i++){
            if(neighbors[i]!=-2){
                MPI_Status status;
                MPI_Status status2; 
                position = 0;
                int flag = 0;
                // Pack and send all of the data
                MPI_Pack(&temperature,  1,  MPI_INT,  packbuf2, packSize, &position, MPI_COMM_WORLD);
                MPI_Pack(&IP_address,  20, MPI_CHAR,  packbuf2, packSize, &position, MPI_COMM_WORLD);
                MPI_Pack(&MAC,         20, MPI_CHAR,  packbuf2, packSize, &position, MPI_COMM_WORLD);
                MPI_Send(packbuf2, packSize, MPI_PACKED, neighbors[i], 0, MPI_COMM_WORLD);
                // Keep looping until a send request is detected
                // If no send after 1 second, we assume that the neighbor node has exitted due to stop signal
                // So in this case, we can also make this sensor node exit as it's neighbor will no longer send anything
                double startTime = MPI_Wtime();
                while(!flag){
                    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status2);
                    double waitTime = MPI_Wtime() - startTime;
                    if(waitTime > 1) break; // Exit after 1 second
                }
                if(stopSignal) break;
                // Unpack all of the data and store it in neighborTemp, neighborIP and neighborMAC
                position = 0;
                MPI_Recv(packbuf2, packSize, MPI_PACKED, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
                MPI_Unpack(packbuf2, packSize, &position, &neighborTemp[i],  1, MPI_INT,  MPI_COMM_WORLD);
                MPI_Unpack(packbuf2, packSize, &position, &neighborIP[i],   20, MPI_CHAR, MPI_COMM_WORLD);
                MPI_Unpack(packbuf2, packSize, &position, &neighborMAC[i],  20, MPI_CHAR, MPI_COMM_WORLD);
            }
        }
        // if(stopSignal) break;
        // Get current time
        char alertTime[dateSize];
        getTimeStamp(alertTime);


        // Initialize and fill an array containing neighboring node's dimensions
        // Also copy each neighbor's temperature for convenience
        int  neighborDetails[4][4];
        int  neighborMatches = 0;
        for(int i=0 ; i<4 ; i++){
            int neighborCoord[2];
            neighborDetails[i][0] = neighborDetails[i][1] = neighborDetails[i][2] = neighborDetails[i][3] = -1;
            if(neighbors[i] != -2){
                if(neighborTemp[i] > TEMP_THRESHOLD || (neighborTemp[i] >= temperature-MATCH_RANGE && neighborTemp[i] <= temperature+MATCH_RANGE)) neighborMatches++;
                MPI_Cart_coords(grid_comm, neighbors[i], ndims, neighborCoord);
                neighborDetails[i][0] = neighbors[i];
                neighborDetails[i][1] = neighborCoord[0];
                neighborDetails[i][2] = neighborCoord[1];
                neighborDetails[i][3] = neighborTemp[i];
            }
        }

        // Get the event time to calculate communication time
        double eventStartTime = MPI_Wtime();

        // Only send to base station if:
        //  1.) Temperature above threshold 
        //  2.) 2+ neighbors have similar temperature 
        //  3.) Stop signal is false
        int sendConditions = temperature > TEMP_THRESHOLD && neighborMatches >= 2 && stopSignal == 0;

        if(sendConditions || rank == 0){
            // Pack all the necessary data
            position = 0;
            MPI_Pack(&sendConditions,   1, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&currentIteration, 1, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&eventStartTime,   1, MPI_DOUBLE, packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&temperature,      1, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&neighborMatches,  1, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&alertTime, dateSize, MPI_CHAR,   packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&coord,            2, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&nodeIPMAC,       40, MPI_CHAR,   packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&neighborDetails, 16, MPI_INT,    packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&neighborIP,      80, MPI_CHAR,   packbuf, packSize, &position, MPI_COMM_WORLD);
            MPI_Pack(&neighborMAC,     80, MPI_CHAR,   packbuf, packSize, &position, MPI_COMM_WORLD);

            // Send packed data to the base station node
            MPI_Send(packbuf, packSize, MPI_PACKED, stationRank, 0, MPI_COMM_WORLD);
        }

        // Increment Current Iteration
        currentIteration++;

        // If there is stop signal, break out of loop else wait for next iteration
        if (stopSignal == 1) break;  
        sleep(sleepTime);
    }

    printf("Rank %d terminated\n", rank);

    MPI_Comm_free(&grid_comm);
	return 0;
}
