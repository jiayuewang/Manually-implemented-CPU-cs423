#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
int factorial(int n){

	int mult = 1;
	for (int i = 2; i<=n; i++){
		mult=mult*i;
	}
	return mult;
}
int main(int argc, char* argv[])
{
	int pid = getpid();
	FILE* file = fopen("/proc/mp1/status","a");
	if(file){
	  fprintf(file,"%d\0",pid);
	  printf("My pid is: %d\n",pid);
	  fclose(file);
	}else{
	  printf("Didn't register to module!\n");
	}
	factorial(1000000000);
	factorial(1000000000);
	factorial(1000000000);
	factorial(1000000000);
	execlp("cat","cat","/proc/mp1/status",NULL);
 // Sets 5 seconds timer interrupt
	return 0;
}
