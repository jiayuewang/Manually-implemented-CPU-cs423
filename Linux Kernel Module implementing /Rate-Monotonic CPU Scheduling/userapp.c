#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Factorial Function
 * Calculates a factorial of a given number
 * For taking up CPU time
 *
 * PARAM n integer
 * RETURN n!
**/
int factorial(int n){

	int mult = 1;
	for (int i = 2; i<=n; i++){
		mult=mult*i;
	}
	return mult;
}

/**
 * Proc Filesystem Function
 * Sends messages to the Kernel Module (Yield, Register, De-register)
 *
 * PARAM PID user application pid
 * PARAM Period user application period
 * PARAM JobProcessTime runtime of one real time loop
 * RETURN 0 if successful, -1 if error
**/
int proc_fs(char message, int PID, unsigned long Period, unsigned long JobProcessTime){
	FILE* file;
	file = fopen("/proc/mp2/status", "a");
	if (file){
		if (message == 'Y'){
			// Yield Function, Notifies Kernel Module that User App is done
			fprintf(file, "Y, %d", PID);
		} else if (message == 'R') {
			// Register Function, Registers app with Kernel Module
			fprintf(file, "R, %d, %lu, %lu", PID, Period, JobProcessTime);
		} else if (message == 'D') {
			// De-Register Function, De-Registers app from Kernel Module
			fprintf(file, "D, %d", PID);
		}
		fclose(file);
		return 0;
	} else{
		printf("Could not open /proc/mp2/status\n");
		return -1;
	}
}

/**
 * Checks /proc/mp2/status to ensure Process is registered
 *
 * PARAM PID user application pid
 * RETURN 1 if process was admitted, 0 otherwise
**/
int check_admission(int PID){
	int cur_PID;
	FILE* f;
	char line[256];
	
	f = fopen("/proc/mp2/status", "r");
	printf("Checking Registration\n");
	
	while(fgets(line, sizeof(line), f)) {
		sscanf(line,"%d%*s",&cur_PID);
		if(cur_PID == PID){
			fclose(f);
			return 1;
		}
	} 

	fclose(f);
	printf("Failed Admission\n");
	return 0;
}

/**
 * Do One Job
 * Use Computation Time
**/
void do_job(){
	factorial(100000000);
}

/**
 * Get current time in microseconds
 *
 * RETURN current microseconds
**/
unsigned long get_usec(){
	struct timeval time;
	while(gettimeofday(&time, NULL));
	return time.tv_sec * 1000000L + time.tv_usec;
}

/**
 * Get average JobProcessTime
 * Runs num_average Jobs and averages the time
 *
 * PARAM num_average integer for number of jobs to run
 * RETURN average runtime per job
**/
unsigned long get_average_runtime(int num_average){
	int i;
	unsigned long t0, runtime, avg_runtime;

	avg_runtime = 0;
	for (i = 0; i < num_average; i++){
		t0 = get_usec();
		do_job();
		runtime = get_usec() - t0;
		avg_runtime = avg_runtime + runtime;
	}

	avg_runtime = avg_runtime / num_average;
	return avg_runtime;	
}

void main(void){
	// Variable Setup
	int jobs;
	int PID;
	unsigned long Period, JobProcessTime;
	unsigned long t0, wakeup_time; 

	// Get Average JobProcessTime
	JobProcessTime = get_average_runtime(10);
	printf("Average Runtime: %lu\n", JobProcessTime);
	
	// Set Period as a multiplicative constant of JobProcessTime
	// multiplicative constant is randomized
	srand(time(NULL));
	int r = (rand() % (5 - 3)) + 3;
	Period = r * JobProcessTime;
	printf("Period: %lu\n", Period);

	// Get Process PID
	PID = getpid();
	printf("PID: %d\n", PID);
	printf("Utilization: %lu\n", JobProcessTime * 1000 / Period);
	// Register Process to Scheduler
	if (proc_fs('R', PID, Period, JobProcessTime) == -1) {
		printf("Error Registering Process\n");
		return;
	}

	// Verify Process was admitted
	if (!check_admission(PID)){
		printf("PID %d failed to register with kernel\n", PID);
		return;
	}
	
	// Get Initial Time before Real-Time Loop
	t0 = get_usec();

	// Set Number of Jobs
	jobs = 10;

        printf("FIRST YIELD\n");

	// Initial Yield Query
	proc_fs('Y', PID, -1, -1);

	printf("STARTING REAL-TIME LOOP\n");

	// Real-Time Loop
	while (jobs > 0){
		// Print Wakeup Time
		wakeup_time = get_usec() - t0;
		//printf("Wakeup Time: %lu\n", wakeup_time);
		
		// Do Job - Use Up Computation Time
		do_job();

		// Print Computation Time
		JobProcessTime = get_usec() - wakeup_time;
		//printf("Computation Time: %lu\n", JobProcessTime);

		// Yield
		proc_fs('Y', PID, -1, -1);
		
		// Decrement Remaining Jobs
		jobs = jobs - 1;
	}

	// De-Register Process
	proc_fs('D', PID, -1, -1);
}
