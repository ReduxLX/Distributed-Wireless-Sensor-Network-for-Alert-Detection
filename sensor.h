/* 
FIT 3143 Assignment 2
Authors [A~Z]:
    Alfons Fernaldy 30127831
    Matthew Khoo 29270294
*/

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

// Function definitions for sensor.h
int slave(MPI_Comm world_comm, MPI_Comm station_comm, int rank, int size);
