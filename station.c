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
extern int    datesize;

extern int    TEMP_LOW;
extern int    TEMP_HIGH;
extern int    TEMP_THRESHOLD;
extern int    MATCH_RANGE;

extern double iterationSleep;
extern int    cummulativeSeed;
extern char   address;
extern char   MAC;

char satelliteTime[30];

void master(int size){
    // Initialize local variables
    int sensorTemp;
    int position;
    MPI_Status status;

    // Initialize a 2D array of row*column to store satellite temperatures
    int satelliteArray[row][column];
    for (int i = 0; i<row; i++){
        for (int j = 0; j<column; j++)
            satelliteArray[i][j] = 0;
    }

    // Start the satellite posix thread and pass in satelliteTemp array
    pthread_t satelliteThread;
    pthread_create(&satelliteThread, NULL, satellite, &satelliteArray);

    int  currentIteration = 0;
    int  neighborMatches;
    double eventStartTime;
    int  messageCount[(size-1)];
    memset(messageCount, 0, (size-1)*sizeof(int));
    char logTime[datesize];
    char alertTime[datesize];
    int  alertNode[2];
    char nodeIPMAC[2][20];
    int  neighborDetails[4][3];
    char neighborIP[4][20];
    char neighborMAC[4][20];
    
    // Listen to incoming requests sent by wsn nodes
    while(currentIteration < maxIterations){
        char packbuf[buffsize];
        printf("Iteration %d\n", currentIteration);
        for(int i=0; i < size - 1; i++){
            position = 0;
            neighborMatches = 0;
            // printf("Rank %d Position: %d\n", 20, position);
            MPI_Recv(packbuf, buffsize, MPI_PACKED, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

            MPI_Unpack(packbuf, buffsize, &position, &eventStartTime, 1, MPI_DOUBLE, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &sensorTemp, 1, MPI_INT, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &alertTime, datesize, MPI_CHAR, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &alertNode, 2, MPI_INT, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &nodeIPMAC, 40, MPI_CHAR, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &neighborDetails, 12, MPI_INT, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &neighborIP, 80, MPI_CHAR, MPI_COMM_WORLD);
            MPI_Unpack(packbuf, buffsize, &position, &neighborMAC, 80, MPI_CHAR, MPI_COMM_WORLD);
            
            // Calcualte the communication time
		    double communicationTime =  MPI_Wtime() - eventStartTime;
            int sourceRank = status.MPI_SOURCE;
            int sourceX = alertNode[0], sourceY = alertNode[1];
            int satelliteTemp = satelliteArray[sourceX][sourceY];
            getTimeStamp(logTime);
            messageCount[sourceRank] += 1; 
            printf("\nNode %02d has temperature %d\n",sourceRank, sensorTemp);
            printf("\tAlert Time: %s | Logged Time: %s\n",alertTime, logTime);
            printf("\tNode Coords: (%d, %d) IP: %s | MAC: %s\n",sourceX, sourceY, nodeIPMAC[0], nodeIPMAC[1]);
            printf("\tNeighbors:\n");
            for(int i=0 ; i<4 ; i++){
                if(neighborDetails[i][0] != -1){
                    int neigborIP = neighborDetails[i][0], neigborMAC = neighborDetails[i][1], neighborTemp = neighborDetails[i][2];
                    if(neighborTemp >= sensorTemp-MATCH_RANGE && neighborTemp <= sensorTemp+MATCH_RANGE) neighborMatches++;
                    printf("\tN%d: (%d, %d, %d)\t", i, neigborIP, neigborMAC, neighborTemp);
                    printf("IP: %s | MAC: %s\n", neighborIP[i], neighborMAC[i]);
                }
            }
            printf("\tInfrared Satellite Reporting Time: %s\n", satelliteTime);
            printf("\tInfrared Satellite Reporting (Celsius): %d\n", satelliteTemp);
            printf("\tInfrared Satellite Reporting Coord: (%d, %d)\n", sourceX, sourceY);
            printf("\tCommunication Time: %f\n", communicationTime);
            printf("\tTotal Messages from Node%02d: %d\n", sourceRank, messageCount[sourceRank]);
            printf("\tNumber of adjacent matches to reporting node: %d\n", neighborMatches);
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
        getTimeStamp(satelliteTime);
        printf("Satellite Iteration %d\n", iteration);
        iteration++;
    }
}



