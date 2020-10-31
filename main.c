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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <mpi.h>

#include "./main.h"
#include "./station.h"
#include "./sensor.h"


/* GLOBAL VARIABLES */
int    stationRank;
int    row, column;
int    rank, size;
double startTime;
int    maxIterations   = -1;
int    stopSignal      = 0;
int    cummulativeSeed = 1;
char   IP_address[20];
char   MAC[20];

void getIPMAC(char* host, char* MAC);

int main(int argc, char *argv[]){
    // Record the start of the simulation
    startTime = MPI_Wtime();

    // Initialize the MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Set base station (master node) rank
    stationRank = size-1;

    /*====================================IMPORTANT====================================*/
    /*     PLEASE SWITCH TO APROACH 2 IF getIPMAC IS CAUSING ERRORS/NOT WORKING        */

    // Approach 1: Get the socket's IP and MAC address
    getIPMAC(IP_address, MAC);

    // Approach 2: Assign a fake simulated IP & MAC address which can vary using the process's rank
    // snprintf(IP_address, 20, "182.253.250.%d", rank);
    // snprintf(MAC, 20, "fc:3f:db:8f:dc:%x", rank%255 > 15 ? rank%255 : (rank%255)+16);
    /*==================================================================================*/
    
    // Check that there are 3 command arguments (main, rows, columns) and that row * column + 1 = size
    // Note: We have chose to let all processes calculate the error value instead of just root node
    // because the alternative involves root node broadcasting error to all other nodes
    int error = 0;
    if(argc != 3 && argc != 4){
        if(rank == stationRank){
            printf("Invalid number of arguments\nFormat should be: mpirun -np <total_processes> -oversubscribe main <row> <column>\n");
            printf("Alternatively to run for x iterations only: mpirun -np <total_processes> -oversubscribe main <row> <column> <x>\n");
        }
        error = -1;
    }else{
        row = atoi(argv[1]);
        column = atoi(argv[2]);
        if(argc == 4) maxIterations = atoi(argv[3]);

        int supportedSize = row * column +1;
        if(supportedSize != size){
            error = -2;
            if(rank == stationRank) 
                printf("Invalid rows and columns in terms of total_processes\nProcesses = row * column +1\n");
        }
    }

    if(rank == stationRank && error==0){
        printf("======================================================================\n");
        printf("Creating a %d x %d cartesian grid containing %d sensor nodes\n", row, column, size-1);
        printf("The WSN will run for %d iterations (-1: infinite)\n", maxIterations);
        printf("Type -1 in commands.txt and save to terminate the WSN manually\n");
        printf("Iteration time interval: %.2f seconds\n", sleepTime);
        printf("By default this is 1 second but it can be changed in main.h\n");
        printf("Warning: Faster time intervals can generate many log entries\n");
        printf("======================================================================\n");
    }
    sleep(1);

    // Exit gracefully using MPI_Finalize() (and not MPI_Abort())
    if(error != 0){
        MPI_Finalize();
        return 0;
    }
   
    // Create a new communicator for the station node
    MPI_Comm station_comm;
    MPI_Comm_split(MPI_COMM_WORLD, rank == stationRank, stationRank, &station_comm);

    // Run specific methods based on node's role
    if (rank == stationRank){
        master();
    }
    else {
	    slave(station_comm); 
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

/* Generates the current timestamp in the form of string "DayName Year-Month-Day Hour:Minute:Second"
*  and assigns this to the char pointer passed in
*/
void getTimeStamp(char* dateStr){
	struct tm ts;
	time_t currentTime;

	// Get time in seconds and use localtime() to find specific time values
	time(&currentTime);
    ts = *localtime(&currentTime);

	// Convert the time to date time string
    strftime(dateStr, dateSize, "%a %Y-%m-%d %H:%M:%S", &ts);
}

// Get the IP and MAC address
void getIPMAC(char* host, char* MAC){
    struct ifaddrs *ifaddr, *ifa;
    int family, s;

    if (getifaddrs(&ifaddr) == -1){
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){
        if (ifa->ifa_addr == NULL)
            continue;

        // Get socket MAC Address 
        if ((ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET)){
            struct sockaddr_ll *sp = (struct sockaddr_ll*)ifa->ifa_addr;
            int offset = 0;
            for (int i=0; i < (sp->sll_halen); i++){
                offset += snprintf(MAC+offset, 30 - offset, "%02x%c", (sp->sll_addr[i]), (i+1!=sp->sll_halen)?':':'\0');
            }
        }
        // Get socket IP address
        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if((strcmp(ifa->ifa_name,"enp0s3")==0) && (ifa->ifa_addr->sa_family==AF_INET)){
            if(s != 0) exit(EXIT_FAILURE);
        }
    }
    // printf("IP Address : <%s>\n", host);
    // printf("MAC Address: <%s>\n", MAC);
    freeifaddrs(ifaddr);
}

