#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int quit = 0;

void sigintHandler(int unusedVar) {
	quit = 1;
}

int main( int argc, char *argv[]) {
	signal(SIGINT, sigintHandler);
	
	while (!quit) {
		pause();
	}

	printf("Exited successfully\n");
	return 0;
}
