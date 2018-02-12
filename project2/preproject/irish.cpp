// irish.cpp
// Ann Keenan (akeenan2)
#include <errno.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <iostream>
#include <string>

int LEVEL = 1;
int PORT = 6000;

// Handler for SIGINT
void sigintHandler(int signum) {
	std::cout << "Exiting..." << std::endl;
	_exit(signum);
}

void exec(std::string command) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
  } else if (pid > 0) {
		std::cout << "Process " << pid << " started in background" << std::endl;
    int status;
    waitpid(pid, &status, 0);
  } else {
    execlp("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
    _exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
	// Catch Control-C via the signal handler
	signal(SIGINT, sigintHandler);

	// parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i++], "-level") == 0) {
			if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "2") == 0) {
				LEVEL = atoi(argv[i]);
			} else {
				std::cout << "Error: Invalid level " << argv[i] << ". Usage: ./irish [--level 1|2]" << std::endl;
				return EXIT_FAILURE;
			}
		} else if (strcmp(argv[i], "-h") == 0) {
			std::cout << "Usage: ./irish [-level 1|2]" << std::endl;
			return EXIT_SUCCESS;
		}
	}

	// Main loop
	while (1) {
		// Prompt for user input
		std::string input;
		std::cout << "> ";
		std::cin >> input;

		// Parse what the user typed
		if (input.compare("bg") == 0) {
			// User input of the command
			std::string command;
			std::cin >> command;
			exec(command);
		} else if (input.compare("quit") == 0) {
			break;  // Exit the loop
		} else {
			std::cout << "Invalid command." << std::endl;
		}
	}
	std::cout << "Exiting..." << std::endl;

	return EXIT_SUCCESS;
}
