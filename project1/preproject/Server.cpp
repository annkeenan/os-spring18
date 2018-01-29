#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void sigintHandler(int signum) {
	printf("Exited successfully\n");
	exit(signum)
}

int main( int argc, char *argv[]) {
	signal(SIGINT, sigintHandler);

	while (1) {
		pause();
	}

	return 0;
}
