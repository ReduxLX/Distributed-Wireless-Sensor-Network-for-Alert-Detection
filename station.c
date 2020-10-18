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
    int    sendConditions;
    int    nodeIteration;
    double eventStartTime;
    int    sensorTemp;
    int    neighborMatches;
    char   alertTime[dateSize];
    int    alertNode[2];
    char   nodeIPMAC[2][20];
    int    neighborDetails[4][4];
    char   neighborIP[4][20];
    char   neighborMAC[4][20];

    // Initialize Counters to track neighbors that match temperatures and total message count
    int  messageCount[(size-1)];
    memset(messageCount, 0, (size-1)*sizeof(int));

    printf("Iteration 1\n");
    // Listen to incoming requests sent by wsn nodes
    while(1){
        MPI_Status status;
        // Check if user has terminated the simulation, break out of loop if yes
        int stopStation = 0;
        // Initialize pack buffer
        char packbuf[packSize];
        int flag = 0;
        position = 0;
        
        // Keep looping here until a send request is received from node or termination signal is sent
        double startTime = MPI_Wtime();
        while(!flag && stopStation != 1){
            MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag, &status);
            stopStation = checkForStopSignal(startTime);
        }
        if(stopStation == 1) break;

        // Receive and unpack all the data sent by one of the sensors
        MPI_Recv(packbuf, packSize, MPI_PACKED, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        MPI_Unpack(packbuf, packSize, &position, &sendConditions,   1, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &nodeIteration,    1, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &eventStartTime,   1, MPI_DOUBLE, MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &sensorTemp,       1, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborMatches,  1, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &alertTime, dateSize, MPI_CHAR,   MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &alertNode,        2, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &nodeIPMAC,       40, MPI_CHAR,   MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborDetails, 16, MPI_INT,    MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborIP,      80, MPI_CHAR,   MPI_COMM_WORLD);
        MPI_Unpack(packbuf, packSize, &position, &neighborMAC,     80, MPI_CHAR,   MPI_COMM_WORLD);
        
        if(nodeIteration > currentIteration){
            currentIteration = nodeIteration;
            printf("Iteration %d\n", currentIteration);
        }

        // Ignore rank 0's signal if rank 0 doesn't meet requirement
        if(sendConditions == 0) continue;

        // Calculate the communication time and get source node's rank, coords and temperature sent
        double communicationTime =  MPI_Wtime() - eventStartTime;
        int sourceRank = status.MPI_SOURCE;
        int sourceX = alertNode[0], sourceY = alertNode[1];
        int satelliteTemp = satelliteArray[sourceX][sourceY];

        // Get current timestamp
        char logTime[dateSize];
        getTimeStamp(logTime);

        // Increment messageCount and totalEvents
        messageCount[sourceRank] += 1; 
        totalEvents += 1;

        // Determine if event is true/false
        // True Event: Temperature > Threshold (80) AND Infrared Temperature > Threshold (80) AND
        //             (>=2 Neighbors Temperature >80 OR Temperature within limit of node's temperature (5))
        // False Event: Otherwise
        int eventType = 0;
        if(sensorTemp > TEMP_THRESHOLD && satelliteTemp > TEMP_THRESHOLD){
            eventType = 1;
            trueEvents++;
        }else{
            falseEvents++;
        }

        // Open the log file in append mode
		FILE *fp;
		fp = fopen("stationLog.txt", "a+");

        // Log the received information
        fprintf(fp, "\n======================================================================\n");
        fprintf(fp, "Iteration: %d\n",currentIteration);
        fprintf(fp, "Logged Time: %s\n",logTime);
        fprintf(fp, "Alert Reported Time: %s\n",alertTime);
        fprintf(fp, "Alert Type: %s\n",eventType ? "True" : "False");
        fprintf(fp, "Reporting Node \t Coords  Temp \t MAC \t\t     IP\n");
        fprintf(fp, "%d \t\t (%d,%d)\t %d   \t %s   %s\n\n", sourceRank, sourceX, sourceY, sensorTemp, nodeIPMAC[1], nodeIPMAC[0]);
        fprintf(fp, "Neighbor Nodes \t Coords  Temp \t MAC \t\t     IP\n");
        for(int i=0 ; i<4 ; i++){
            if(neighborDetails[i][0] != -1){
                int neighborRank = neighborDetails[i][0], neighborX    = neighborDetails[i][1], 
                    neighborY    = neighborDetails[i][2], neighborTemp = neighborDetails[i][3];
                fprintf(fp, "%d \t\t (%d,%d)\t %d   \t %s   %s\n", neighborRank, neighborX, neighborY, neighborTemp, neighborMAC[i], neighborIP[i]);
            }
        }
        fprintf(fp, "\nInfrared Satellite Reporting Time: %s\n", satelliteTime);
        fprintf(fp, "Infrared Satellite Reporting (Celsius): %d\n", satelliteTemp);
        fprintf(fp, "Infrared Satellite Reporting Coord: (%d, %d)\n", sourceX, sourceY);

        fprintf(fp, "\nCommunication Time: %f\n", communicationTime);
        fprintf(fp, "Total Messages from Node %d: %d\n", sourceRank, messageCount[sourceRank]);
        fprintf(fp, "Number of adjacent matches to reporting node: %d\n", neighborMatches);
        fprintf(fp, "======================================================================\n");
        fclose(fp);
    }
    // Log final station report after termination
    FILE *fp;
    fp = fopen("stationLog.txt", "a+");
    fprintf(fp, "======================================================================\n");
    fprintf(fp, "STATION TERMINATION REPORT\n");
    fprintf(fp, "Terminated at Iteration %d\n",currentIteration);
    fprintf(fp, "Terminated Manually? %s\n", stopSignal ? "Yes" : "No");
    fprintf(fp, "Total Elapsed Time: %f seconds\n", MPI_Wtime() - startTime);
    fprintf(fp, "Total Recorded Events: %d\n", totalEvents);
    fprintf(fp, "True Events: %d\n", trueEvents);
    fprintf(fp, "False Events: %d\n", falseEvents);
    fprintf(fp, "======================================================================\n");
    fclose(fp);
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
    // If no send requests detected after 3 seconds, send termination signal
    double waitTime = MPI_Wtime() - startTime;
    if(maxIterations != -1 && (waitTime > 3 || currentIteration >= maxIterations+1)) return 1;
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


