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
extern int    rank, size;
extern double startTime;
extern int    maxIterations;
extern int    stopSignal;

/* Initialize new Global Variables */
char satelliteTime[30];
int  satelliteIteration = 0;
int  currentIteration = 1;

int checkForStopSignal(double startTime);

void master(){
    // Initialize end-of-report counters
    int totalEvents = 0;
	int trueEvents = 0;
    int falseEvents = 0;

    // Initialize a 2D array of row*column to store satellite temperatures
    int satelliteArray[row][column];
    for (int i = 0; i<row; i++){
        for (int j = 0; j<column; j++)
            satelliteArray[i][j] = 0;
    }

    // Start the satellite posix thread and pass in satelliteTemp array
    pthread_t satelliteThread;
    pthread_create(&satelliteThread, NULL, satellite, &satelliteArray);

    // MPI_Status status;
    int position;

    // Buffers for packed data (in order of appearance)
    double eventStartTime;
    int sensorTemp;
    char alertTime[dateSize];
    int  alertNode[2];
    char nodeIPMAC[2][20];
    int  neighborDetails[4][3];
    char neighborIP[4][20];
    char neighborMAC[4][20];

    // Initialize Counters to track neighbors that match temperatures and total message count
    int  neighborMatches;
    int  messageCount[(size-1)];
    memset(messageCount, 0, (size-1)*sizeof(int));

    // Listen to incoming requests sent by wsn nodes
    while(1){
        // Check if user has terminated the simulation, break out of loop if yes
        int stopStation = 0;
        // Initialize pack buffer
        char packbuf[packSize];
        MPI_Status status;
        position = 0;
        neighborMatches = 0;
        int flag = 0;
        // Receive and unpack all the data sent by the sensor
        double startTime = MPI_Wtime();
        while(!flag && stopStation != 1){
            MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
            stopStation = checkForStopSignal(startTime);
        }
        if(stopStation == 1) break;
        printf("Iteration %d\n", currentIteration);
        MPI_Recv(packbuf, packSize, MPI_PACKED, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        MPI_Unpack(packbuf, packSize, &position, &currentIteration, 1, MPI_INT, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &eventStartTime, 1, MPI_DOUBLE, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &sensorTemp, 1, MPI_INT, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &alertTime, dateSize, MPI_CHAR, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &alertNode, 2, MPI_INT, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &nodeIPMAC, 40, MPI_CHAR, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborDetails, 12, MPI_INT, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborIP, 80, MPI_CHAR, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborMAC, 80, MPI_CHAR, MPI_COMM_WORLD);
        
        // Calculate the communication time and get source node's rank, coords and temperature sent
        double communicationTime =  MPI_Wtime() - eventStartTime;
        int sourceRank = status.MPI_SOURCE;
        int sourceX = alertNode[0], sourceY = alertNode[1];
        int satelliteTemp = satelliteArray[sourceX][sourceY];

        // Get current timestamp
        char logTime[dateSize];
        getTimeStamp(logTime);

        // Increment messageCount
        messageCount[sourceRank] += 1; 

        // Log the received information
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
        printf("\n");
    }
    // Log final station report after termination
    printf("======================================================================\n");
    printf("STATION TERMINATION REPORT\n");
    printf("Terminated at Iteration %d\n",currentIteration);
    printf("Terminated Manually? %s\n", stopSignal ? "Yes" : "No");
    printf("Total Elapsed Time: %f seconds\n", MPI_Wtime() - startTime);
    printf("Total Recorded Events: %d\n", totalEvents);
    printf("True Events: %d\n", trueEvents);
    printf("False Events: %d\n", falseEvents);
    printf("======================================================================\n");
}

/* The Satellite routine which runs indefinitely until station node is terminated
 * It takes in an a 2D array representing all sensor nodes and generates random temperature for each node per iteration
 */
void* satellite(void* arg){
    int (*array)[column] = arg;
    while(1){
        for (int i = 0; i<row; i++){
            for (int j = 0; j<column; j++){
                int sat_temperature = randomValue(TEMP_LOW, TEMP_HIGH, stationRank);
                array[i][j] = sat_temperature;
            }
        }
        getTimeStamp(satelliteTime);
        satelliteIteration++;
        sleep(sleepTime);
    }
}

/* Open and Read a text file called commands.txt, If "-1 is found", send a stop signal to all sensor nodes */
int checkForStopSignal(double startTime){
    double waitTime = MPI_Wtime() - startTime;
    // printf("Wait Time: %f\n", waitTime);
    // printf("current %d | max %d", currentIteration, maxIterations);
    if(waitTime > 2 || (currentIteration >= maxIterations+1 && maxIterations != -1)) return 1;
    // Read and trim text from commands.txt to remove "\n"
    FILE *f = fopen("commands", "r");
    char userInput[10];
    fgets(userInput, 10, f);
    strtok(userInput, "\n");

    // If "-1" detected then send non-blocking send requests to all sensor nodes with stopSignal=1
    if(strcmp(userInput,"-1")==0){
        printf("User Terminating Input Detected\n");
        stopSignal = 1;
        MPI_Request send_request[size];
        MPI_Status receive_status[size];
        int numberOfReq = 0;
        // Send a message to each node to end the iteration
        for (int i = 0; i < size; i++){
            MPI_Isend(&stopSignal, 1, MPI_INT, i, 3, MPI_COMM_WORLD, &send_request[numberOfReq]);
            numberOfReq+=1;
        }
        // Wait until all messages are sent to all nodes
	    MPI_Waitall(numberOfReq , send_request, receive_status);
        return 1;
    }
    fclose(f);
    return 0;
}


