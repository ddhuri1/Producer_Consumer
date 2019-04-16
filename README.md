Devina Sachin Dhuri : ddhuri1@binghamton.edu

1. CONTENTS OF THE PACKAGE:
-------------
README		-this file
producer.c 	-Code for producer
consumer.c 	-Code for consumer
numpipe.c 	-Kernel module code
makefile 	-Makefile to compile the kernel module

-------------
1. OBJECTIVES:
-------------
 Implement a kernel-level pipe for exchanging numbers among user-level processes. 
 Learn about concurrency, synchronization, and various kernel primitives.
 
-------------
2. DESCRIPTION:
-------------
PART C:

	*********Implementation of a character device and inserting it as a kernel module*********
	The semaphore-version of the producer consumer problem is used.
	Two types of semaphores are used:
		i) A binary semaphor to lock the queue : Mutex
		ii) A regular sem to check if queue is empty or full and thus block.
		
	The earlier problem of synchronization is solved i.e if the pipe is full the producer is blocked till there is an empty slot available. Once a consumer reads the data, it will notify the producer and wake them up to write to the pipe. 
	If the pipe is empty, the consumer is blocked till a data is available to be read. Once data is written by producer, the consumer is notified and woken up to now read the data. 
	
	When data are not available process calls down_interruptible() and blocks. Once resources are freed up() is called, and blocked process is unblocked. 
	down_interruptible() allows processes that receive a signal while being blocked on a semaphore to give up the "down" operation. 
	
	In the producer code, before it writes in the queue, it will check if the queue is full using the down(sem) operation and reducing the count of mutex to 0 indicating the queue is currently bein used. Then it writes and releases the mutex by incrementing its count to 1. 
	
	In the consumer code, before t reads, it will check if queue is empty by using the down operation on the empty condition and reducing the count of mutex to 0 indicating the queue is currently being used. Then it reads and releases the mutex by incrementing its count to 1. 
	
	Once a data is read from the queue it is removed from the queue. Thus,each number produced is read only once by a consumer and no number is lost. 
	All the producers and consumers make progress without deadlocks.
	
	The sem_init(), will be used to initialize the semaphore before using it. Sem_wait and sem_post(), are used to operate on the semaphores. When it is no longer in use, sem_destroy() is used to deallocate memory allocated to the semaphore.
-------------
3. COMPILATION STEPS:

//Compile kernel module
1. make

//Insert the kernel module
2. sudo insmod numpipe.ko pipe_size=N (Replace N with desired Buffer size.)

//Run Producer Code
3. gcc -o p1 producer.c

//Run Consumer Code
4.gcc -o c1 consumer.c

//Run executable with the kernel module
5. sudo ./p1 /dev/numpipe (numpipe is the name of my kernel module)
6. sudo ./c1 /dev/numpipe

//Delete object and executables generated
7. make clean

//Remove module
8. rmmod numpipe.ko




