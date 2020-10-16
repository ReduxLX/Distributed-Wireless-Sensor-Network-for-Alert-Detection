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
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "./main.h"
#include "./station.h"
#include "./sensor.h"


/* GLOBAL VARIABLES */
int    stationRank;
int    row, column;
int    maxIterations   = 2;
int    buffsize        = 500;
int    datesize        = 30;

int    TEMP_LOW        = 60;
int    TEMP_HIGH       = 100;
int    TEMP_THRESHOLD  = 80;
int    MATCH_RANGE     = 5;

double iterationSleep  = 1;
int    cummulativeSeed = 1;
char   address[20];
char   MAC[]           = "fc:3f:db:8f:dc:15";


int main(int argc, char *argv[]){
    int rank, size;
    double start_time = MPI_Wtime();

    // Initialize the MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Set base station (master node) rank
    stationRank = size-1;

    // Assign a simulated IP address to each node
    snprintf(address, 20, "182.253.250.%d", rank);
    
    // Check that there are 3 command arguments (main, rows, columns) and that row * column + 1 = size
    // Note: We have chose to let all processes calculate the error value instead of just root node
    // because the alternative involves root node broadcasting error to all other nodes
    int error = 0;
    if(argc != 3){
        if(rank == stationRank) 
            printf("Invalid number of arguments\nFormat should be: mpirun -np <total_processes> -oversubscribe main <row> <column>\n");
        error = -1;
    }else{
        row = atoi(argv[1]);
        column = atoi(argv[2]);
        int supportedSize = row * column +1;
        if(supportedSize != size){
            error = -2;
            if(rank == stationRank) 
                printf("Invalid rows and columns in terms of total_processes\nProcesses = row * column +1\n");
        }
    }

    // Exit gracefully using MPI_Finalize() and not MPI_Abort()
    if(error != 0){
        MPI_Finalize();
        return 0;
    }
   
    // Create a new communicator for the station node
    MPI_Comm station_comm;
    MPI_Comm_split(MPI_COMM_WORLD, rank == stationRank, stationRank, &station_comm);

    // Run specific methods based on node's role
    if (rank == stationRank){
        master(size);
    }
    else {
	    slave(station_comm, rank, size); 
    }

    // Finalize the MPI program
    MPI_Comm_free(&station_comm);
    MPI_Finalize();

    return 0;
}

/* Generates a random temperature value between TEMP_HIGH and TEMP_LOW 
*  To ensure processes don't generate the same random values for the same time,
*  we add its rank to cummulativeSeed for each generated number
*/
int randomValue(int low, int high, int rank){
    // Reset cummulativeSeed if it approaches int upper limit
    if(cummulativeSeed > 2000000000) cummulativeSeed = 1;
    cummulativeSeed += (rank*rank + 2);
    unsigned int seed = time(0) * cummulativeSeed;
    int randomVal = low + (rand_r(&seed) % (high-low+1));
    return randomVal;
}

void getTimeStamp(char* buf){
	struct tm ts;
	time_t currentTime;

	// Get time in seconds and use localtime() to find specific time values
	time(&currentTime);
    ts = *localtime(&currentTime);

	// Convert the time to date time string
    strftime(buf, datesize, "%a %Y-%m-%d %H:%M:%S", &ts);
}

