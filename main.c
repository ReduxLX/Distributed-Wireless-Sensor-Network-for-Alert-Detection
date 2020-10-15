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

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1
#define TEMP_LOW 60
#define TEMP_HIGH 100
#define TEMP_THRESHOLD 80

/* GLOBAL VARIABLES */
int    stationRank;
int    maxIterations = 2;
double iterationSleep = 1;
int    row, column;
int    cummulativeSeed = 1;
char   address[20];
char   MAC[] = "fc:3f:db:8f:dc:15";

void master(MPI_Comm world_comm, int size);
int slave(MPI_Comm world_comm, MPI_Comm station_comm, int rank, int size);
int randomValue(int low, int high, int rank);
void getTimeStamp(char* buf, int size);

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
    snprintf(address, 20, "182.253.250.%d", 50+rank);
    
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
        master(MPI_COMM_WORLD, size);
    }
    else {
	    slave(MPI_COMM_WORLD, station_comm, rank, size); 
    }

    // Finalize the MPI program
    MPI_Comm_free(&station_comm);
    MPI_Finalize();

    return 0;
}

void* satellite(void* arg){
    int (*array)[column] = arg;
    int iteration = 0;
    while(iteration < maxIterations){
        int sat_temperature = randomValue(TEMP_LOW, TEMP_HIGH, stationRank);
        int randomRow = randomValue(0, row-1, stationRank);
        int randomCol = randomValue(0, column-1, stationRank);
        array[randomRow][randomCol] = sat_temperature;
        printf("Satellite Iteration %d | Row %d | Col %d | Temperature %d\n", iteration, randomRow, randomCol, array[randomRow][randomCol]);
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


int slave(MPI_Comm world_comm, MPI_Comm station_comm, int rank, int size){
    MPI_Comm grid_comm;
    int ndims = 2;
    int reorder = 1;
    int dims[2] = {row, column};
    int period[2] = {0, 0};
    int coord[2];
    int neighbors[4];
    int nrows, ncols;
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
        // Generate temperature and send to station
        int temperature = randomValue(TEMP_LOW, TEMP_HIGH, rank);
        // printf("Rank %d: %d\n", rank, temperature);
        MPI_Send(&temperature, 1, MPI_INT, stationRank, 0, world_comm);
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

/* Generates a random temperature value between TEMP_HIGH and TEMP_LOW 
*  To ensure processes don't generate the same random values, we use the process's rank
*  as a product of cumulative seed which is multiplied with the time
*/
int randomValue(int low, int high, int rank){
    cummulativeSeed *= (rank + 2); // +2 so cummulativeSeed of ranks 0 and 1 can grow
    unsigned int seed = time(0) * cummulativeSeed;
    int randomVal = low + (rand_r(&seed) % (high-low+1));
    return randomVal;
}

void getTimeStamp(char* buf, int size){
	struct tm ts;
	time_t currentTime;

	// Get time in seconds and use localtime() to find specific time values
	time(&currentTime);
    ts = *localtime(&currentTime);

	// Convert the time to date time string
    strftime(buf, size, "%a %Y-%m-%d %H:%M:%S", &ts);
}
