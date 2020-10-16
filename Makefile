ALL: main

init : main.c
	mpicc -fopenmp main.h main.c sensor.c station.c -o main

run:
	mpirun -np 21 -oversubsrcibe main 5 4

clean :
	/bin/rm -f  *.o

