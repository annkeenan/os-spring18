// Server.cpp
// Ann Keenan (akeenan2)
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Signal handler
void sigintHandler(int signum) {
	printf("Exited successfully\n");
	exit(signum)
}

// Main body
int main( int argc, char *argv[]) {
	// Register the signal handler
	signal(SIGINT, sigintHandler);

	// Wait for the signal to be caught
	while (1) {
		pause();
	}

	return 0;
}
