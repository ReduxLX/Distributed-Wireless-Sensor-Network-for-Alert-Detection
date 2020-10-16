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
#include "./station.h"

/* Import Global Variables */
extern int    stationRank;
extern int    row, column;
extern int    maxIterations;

extern int    TEMP_LOW;
extern int    TEMP_HIGH;
extern int    TEMP_THRESHOLD;

extern double iterationSleep;
extern int    cummulativeSeed;
extern char   address;
extern char   MAC;

int randomValue(int low, int high, int rank);
void getTimeStamp(char* buf, int size);

void* satellite(void* arg){
    int (*array)[column] = arg;
    int iteration = 0;
    while(iteration < maxIterations){
        for (int i = 0; i<row; i++){
            for (int j = 0; j<column; j++){
                int sat_temperature = randomValue(TEMP_LOW, TEMP_HIGH, stationRank);
                array[i][j] = sat_temperature;
            }
        }
        printf("Satellite Iteration %d\n", iteration);
        iteration++;
    }
}

void master(MPI_Comm world_comm, int size){
    MPI_Status status;
    int sensorTemp;
    // Initialize a 2D array of row*column to store satellite temperatures
    int satelliteTemp[row][column];
    for (int i = 0; i<row; i++){
        for (int j = 0; j<column; j++)
            satelliteTemp[i][j] = 0;
    }

    // Start the satellite posix thread and pass in satelliteTemp array
    pthread_t satelliteThread;
    pthread_create(&satelliteThread, NULL, satellite, &satelliteTemp);

    // Listen to incoming requests sent by wsn nodes
    int currentIteration = 0;
    char timeStamp[30];
    while(currentIteration < maxIterations){
        printf("Iteration %d\n", currentIteration);
        for(int i=0; i < size - 1; i++){
            MPI_Recv(&sensorTemp, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &status);
		    getTimeStamp(timeStamp, 30);
            printf("%s - Node %02d has temperature %d\n",timeStamp, status.MPI_SOURCE, sensorTemp);
        }
        printf("\n");
        currentIteration++;
        sleep(iterationSleep);
    }
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < column; j++) {
            printf("%d ", satelliteTemp[i][j]);
        }
        printf("\n");
    }
}
