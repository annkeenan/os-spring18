// mandelmovie.c
// Ann Keenan (akeenan2)

#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_IMAGES 50
#define MAX_PROCESSES MAX_IMAGES
#define MAXBUF 256
#define MAX_ZOOM 2
#define MIN_ZOOM 0.00001

void buildCommand(char **, double, int);

int main(int argc, char *argv[]) {
  // Ensure valid execution of the program
  int numProcesses = 1;
  if (argc == 1) {
    printf("No number of processes specified. Defaulting to %d.\n", numProcesses);
  } else if (argc > 2) {
    printf("Usage: ./mandelmovie n\n    n    number of processes\n");
    return EXIT_FAILURE;
  } else if (!isdigit(argv[1][0]) || atoi(argv[1])==0) {
    printf("ERROR: '%s' NaN or less than 1. Defaulting to 1.\n", argv[1]);
  } else if (atoi(argv[1]) > MAX_PROCESSES) {
    printf("ERROR: Input exceed max number of processes allowed: %d. Defaulting to 1.\n", MAX_PROCESSES);
  } else {
    numProcesses = atoi(argv[1]);
  }

  double s = 2;
  double base = exp(log(MIN_ZOOM/MAX_ZOOM)/(MAX_IMAGES-1));
  int n = 0;  // number of running processes
  pid_t pids[MAX_IMAGES];

  int i;
  for (i = 0; i < MAX_IMAGES; i++) {
    // Execute n number of processes
    if (n < numProcesses) {
      // Fork a child process and execute the command
      pids[i] = fork();
      n++;

      // Error
      if (pids[i] == -1) {
    		printf("ERROR: Failure to fork\n");

      // Child process
      } else if (pids[i] == 0) {
        char *command[18];
        s = 2 * pow(base, i);
        buildCommand(command, s, i+1);

        // printf("DEBUG: Started process %d.\n", pid);
        execvp(command[0], command);
    		printf("ERROR: Failure starting command for i=%d\n", i+1);
        exit(EXIT_FAILURE);

      // Parent process
      } else {
        // Wait until a process finishes
        while (n >= numProcesses) {
          int status, wpid;
          wpid = wait(&status);

      		if (status) {  // error
    				printf("ERROR: Process %d exited with status %d.\n", wpid, status);
            // exit(EXIT_FAILURE);
    			} else if (wpid != 0) {  // success in waiting
            // printf("DEBUG: Process %d completed with status %d.\n", wpid, status);
            n--;
    			}
        }
      }
    }
  }

  // Clean up remaining processes
  while (n > 0) {
    int status, wpid;
    wpid = wait(&status);
    if (status) {  // error
      printf("ERROR: Process %d exited with status %d.\n", wpid, status);
    }
    n--;
  }

  return EXIT_SUCCESS;
}

// Build the command to be run
void buildCommand(char **cmd, double s, int i) {
  char filename[13];
  char s_str[10];
  snprintf(filename, 13, "mandel%d.bmp", i);
  snprintf(s_str, 10, "%f", s);

// ./mandel -x 0.2910234 -y -0.0164365 -s .00001 -m 1000 -W 700 -H 700
  cmd[0] = strdup("./mandel");
	cmd[1] = strdup("-x");
	cmd[2] = strdup("0.2910234");
	cmd[3] = strdup("-y");
	cmd[4] = strdup("-0.0164365");
	cmd[5] = strdup("-s");
	cmd[6] = strdup(s_str);
	cmd[7] = strdup("-m");
	cmd[8] = strdup("4000");
	cmd[9] = strdup("-W");
	cmd[10] = strdup("700");
	cmd[11] = strdup("-H");
	cmd[12] = strdup("700");
	cmd[13] = strdup("-o");
	cmd[14] = strdup(filename);
  // Number of threads - update for combination
  cmd[15] = strdup("-n");
  cmd[16] = strdup("1");
	cmd[17] = NULL;
}
