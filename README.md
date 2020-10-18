# Distributed-Wireless-Sensor-Network-for-Alert-Detection
A distributed computing system which aims to simulate a wireless sensor network using parallel computing.
The wireless sensor network comprises of sensor nodes and a base station node with a satellite thread simulated using posix threads.
The satellite thread generates a random temperature for each sensor node each iteration.
Each sensor node periodically generates a random temperature which is compared with the sensor neighbors.

Sensor nodes only sends a message to the base station if the following conditions are met:
- Temperature generated > TEMP_THRESHOLD (80)
- At least 2 neighboring nodes have temperatures > TEMP_THRESHOLD OR temperatures within LIMIT of 5 degrees

Once these requirements are met, the nodes pack all information needed and send them to the station as an event.
The station then labels events as "True" if the satellite temperature reading for that node > TEMP_THRESHOLD, otherwise "FALSE".
All Events are logged in a text file "stationLog.txt".

The station keeps receiving events from sensors and logging them until the following:
- Sensors have run for x iterations specified by user
- User terminates it early by writing "-1" in commands.txt

Upon receiving the termination signal, the station will send a stop signal to all sensor nodes.
Finally it proceeds to log an end-report to "stationLog.txt" before terminating.

# Instructions
### Option 1:
Compile files <br>
`mpicc -fopenmp main.h main.c sensor.c station.c -o main` <br><br>
Run network indefinitely <br>
`mpirun -np 21 -oversubscribe main 5 4` <br><br>
Run network for 10 iterations <br>
`mpirun -np 21 -oversubscribe main 5 4 10` <br><br>

### Option 2: 
Use "make" to automatically compile and run the program. <br>
Change contents of MakeFile to adjust input parameters.

# Interesting Project Facts
All sensor nodes make an MPI_Irecv call at the very start and this is to be able to catch base station's stop signal any time.
This makes it possible for users to terminate the wsn early using commands.

Base Station doesn't follow the iteration loop like sensors, instead it runs indefinitely. This is because the station
cannot predict how many send requests it will get from sensors. 
To solve this problem, MPI_Iprobe is used which will check when send requests are available in a non-blocking fashion. 
This solves the blocking deadlock issues but introduces additional issues:
- Base Station doesn't know which iteration the sensor nodes are running so it must rely on events sent to keep track of latest iteration
- However in a scenario where no events are sent (temp < 80), the base station cannot terminate properly as it is stuck in iteration 1

If the user specifies x iterations for the wsn to run, the base station automatically terminates after 3 seconds if it doesn't receive any events.
This avoids deadlock scenarios which can happen at the last iteration.
The base station doesn't know how many nodes will send an event during an iteration 
The solution is to make it wait 3 seconds under the assumption that if no more nodes send any events in 3 seconds then surely it can terminate.

**But what if no events are sent in 3 seconds due to random nature of temperature generation ???**

We assign one sensor node (say node 0) to always send a message to base station for each iteration to update the station with the current node iteration.
In real world scenarios this can fail if the sensor node goes offline however this project's goals doesn't extend fault tolerance so we can assume
that our selected node won't fail for the duration of network.
Having one station send messages constantly minimizes unnecessary send requests whilst making sure base station stays up to date with iterations.

After the last iteration, node 0 will stop sending messages, base station 3 seconds timer will run out and it can safely send termination message.

