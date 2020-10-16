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
extern int    buffsize;

extern int    TEMP_LOW;
extern int    TEMP_HIGH;
extern int    TEMP_THRESHOLD;

extern double iterationSleep;
extern int    cummulativeSeed;
extern char   address;
extern char   MAC;

int  randomValue(int low, int high, int rank);
void getTimeStamp(char* buf, int size);
void startSatellite();

void master(int size){
    // Initialize local variables
    int sensorTemp;
    int position;
    MPI_Status status;

    startSatellite();

    // Listen to incoming requests sent by wsn nodes
    int currentIteration = 0;
    char timeStamp[30];
    
    while(currentIteration < maxIterations){
        char packbuf[buffsize];
        printf("Iteration %d\n", currentIteration);
        for(int i=0; i < size - 1; i++){
            position = 0;
            // printf("Rank %d Position: %d\n", 20, position);
            MPI_Recv(packbuf, buffsize, MPI_PACKED, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            MPI_Unpack(packbuf, buffsize, &position, &sensorTemp, 1, MPI_INT, MPI_COMM_WORLD);
            getTimeStamp(timeStamp, 30);
            printf("%s - Node %02d has temperature %d\n",timeStamp, status.MPI_SOURCE, sensorTemp);
        }
        printf("\n");
        currentIteration++;
        sleep(iterationSleep);
    }

    // for (int i = 0; i < row; i++) {
    //     for (int j = 0; j < column; j++) {
    //         printf("%d ", satelliteTemp[i][j]);
    //     }
    //     printf("\n");
    // }
}

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

void startSatellite(){
    // Initialize a 2D array of row*column to store satellite temperatures
    int satelliteTemp[row][column];
    for (int i = 0; i<row; i++){
        for (int j = 0; j<column; j++)
            satelliteTemp[i][j] = 0;
    }

    // Start the satellite posix thread and pass in satelliteTemp array
    pthread_t satelliteThread;
    pthread_create(&satelliteThread, NULL, satellite, &satelliteTemp);
}


