// irish.cpp
// Ann Keenan (akeenan2)

#include <errno.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <zmq.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <map>

#define MAXBUF 128

int LEVEL = 1;
int PORT = 6000;

std::map<int, std::string> processes;

// Return pid of completed process
int waitProcesses() {
	int status, wpid;
	// Loop through list of started child processes
	for (std::map<int, std::string>::iterator p = processes.begin(); p != processes.end(); ++p) {
		wpid = waitpid(p->first, &status, WNOHANG);
		if (wpid == 0) {  // process not complete
			continue;
		} else {
			if (wpid == -1) {  // error
				std::cout << "Error: Failure waiting for process " << p->first << std::endl;
			} else if (wpid != 0) {  // success in waiting
				std::cout << "DEBUG: Process " << p->first << " completed" << std::endl;
			}
			return p->first;
		}
	}
	return -1;
}

void sigintHandler(int signum) {
	std::cout << "Exiting..." << std::endl;
	// Clean up all started child processes
	int pid;
	while (!processes.empty()) {
		if ((pid = waitProcesses()) != -1) {
			processes.erase(pid);
		}
	}
	_exit(signum);
}

// Send signal signum to process pid, return success
bool sendSignal(int pid, int signum) {
	if (processes.find(pid) == processes.end()) {
		std::cout << "Error: Process " << pid << " does not exist" << std::endl;
		return false;
	}
	if (kill(pid, signum) == -1) {
    std::cout << "Error: Failure to send signal " << signum << " to process " << pid << std::endl;
    return false;
  }
	return true;
}


// Return the signal string as an int, returns -1 on signal sending error
int handleSignal(int pid, std::string sig) {
	std::map<int, std::string>::iterator p;
	int signum;

	if ((p = processes.find(pid)) == processes.end()) {
		std::cout << "Error: Process " << pid << " does not exist" << std::endl;
		return 1;
	}

	std::string oldStatus;
	oldStatus = p->second;

	if (sig == "SIGHUP") {
		signum = SIGHUP;
		p->second = "Terminated";
	} else if (sig == "SIGINT") {
		signum = SIGINT;
		p->second = "Terminated";
	} else if (sig == "SIGQUIT") {
		signum = SIGQUIT;
		p->second = "Terminated";
	} else if (sig == "SIGSTOP") {
		if (p->second == "stopped") {
			std::cout << "Process " << pid << " already stopped" << std::endl;
			return 1;
		}
		signum = SIGSTOP;
		p->second = "Stopped";
	} else if (sig == "SIGALRM") {
		signum = SIGALRM;
		p->second = "Stopped";
	} else if (sig == "SIGCONT") {
		if (p->second == "Running") {
			std::cout << "Process " << pid << " already running" << std::endl;
			return 1;
		}
		p->second = "Running";
		signum = SIGCONT;
	} else {
		std::cout << "Error: Signal " << sig << " is not supported" << std::endl;
		return 1;  // unsupported signal
	}

	if (!sendSignal(pid, signum)) {
		std::cout << "Error: Failure to send signal " << sig << " on process " << pid << std::endl;
		p->second = oldStatus;
		return -1;
	} else {
		if (p->second.compare("terminated") == 0) {
			std::cout << "DEBUG: Process " << p->first << " terminated" << std::endl;
			processes.erase(p);
		}
		return 0;
	}
}

int exec(std::string command) {
	// Parse command into list of arguments
	std::istringstream iss(command);
	std::vector<std::string> buf;
	for(std::string l; iss >> l; ) {
		buf.push_back(l);
	}
	std::vector<char*> args;
	for (std::vector<std::string>::iterator i = buf.begin(); i != buf.end(); ++i) {
		args.push_back(const_cast<char*>(i->c_str()));
	}

  pid_t pid = fork();
  if (pid == -1) {
		std::cout << "Error: Failure to fork" << std::endl;
  } else if (pid > 0) {
		std::cout << "Process " << pid << " started in background" << std::endl;
		processes.insert(std::pair<int, std::string>{pid, "Running"});
  } else {
    execvp(args.data()[0], args.data());
		std::cout << "Error: Failure starting command " << command << std::endl;
    return 1;
		// _exit(EXIT_FAILURE);
  }
	return 0;
}

int main(int argc, char *argv[]) {
	// Catch Control-C via the signal handler
	signal(SIGINT, sigintHandler);

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
		// Print help message
			std::cout << "Usage: ./irish [-level 1|2]" << std::endl;
			return EXIT_SUCCESS;
		} else if (strcmp(argv[i++], "-level") == 0) {
			if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "2") == 0) {
			// Set the level, default 1
				LEVEL = atoi(argv[i]);
			} else {
			// Invalid command line option
				std::cout << "Error: Invalid level " << argv[i] << std::endl
									<< "Usage: ./irish [--level 1|2]" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}

	// Main loop
	while (1) {
		// Prompt for user input
		std::string input;

		std::cout << "> ";
		std::cin >> input;

		// Wait and cleanup any completed processes
		int pid;
		while ((pid = waitProcesses()) != -1) {
			processes[pid] = "Completed";
		}

		// Start a process in the background
		if (input.compare("bg") == 0) {
			std::string line;

			std::getline(std::cin, line);

			// Execute the command
			exec(line);
		} else if (input.compare("list") == 0) {
			if (processes.empty()) {
				std::cout << "No subprocesses." << std::endl;
			} else {
				std::cout << processes.size() << "Background Process(es)." << std::endl;
				int n = 1;
				for (std::map<int, std::string>::iterator p=processes.begin(); p!=processes.end(); ++p) {
	        std::cout << "(" << n++ << ")" << " PID=" << p->first << " State=" << p->second << std::endl;
				}
			}

		// Bring process to foreground and wait to complete
		} else if (input.compare("fg") == 0) {
			int pid, status, wpid;
	  	std::cin >> pid;
			std::map<int, std::string>::iterator p;

			// Process is either completed or does not exist
			if ((p = processes.find(pid)) == processes.end()) {
				std::cout << "Error: Process " << pid << " does not exist" << std::endl;
				continue;
			} else if (p->second == "Completed") {
				std::cout << "Process " << pid << " completed" << std::endl;
				processes.erase(p);
				continue;
			} else if (p->second == "Stopped") {
				std::cout << "Process " << pid << " stopped" << std::endl;
				processes.erase(p);
				continue;
			} else if (p->second == "Terminated") {
				std::cout << "Process " << pid << " terminated" << std::endl;
				processes.erase(p);
				continue;
			}

			// Block until the running process is complete
	    wpid = waitpid(pid, &status, 0);
			if (wpid == -1) {
				std::cout << "Error: Failure to wait on process " << pid << std::endl;
				continue;
			}
			std::cout << "DEBUG: Process " << pid << " complete" << std::endl;

		// Send signal to a process
		} else if (input.compare("signal") == 0) {
			int pid;
			std::string sig;
			std::cin >> pid >> sig;
			handleSignal(pid, sig);

		// Stop a process
		} else if (input.compare("stop") == 0) {
			int pid;
			std::cin >> pid;
			if (sendSignal(pid, SIGSTOP)) {
				processes[pid] = "stopped";
				std::cout << "Process " << pid << " stopped." << std::endl;
			}

		// Start a process again
		} else if (input.compare("continue") == 0) {
			int pid;
			std::cin >> pid;
			if (sendSignal(pid, SIGCONT)) {
				processes[pid] = "Running";
				std::cout << "Process " << pid << " continued." << std::endl;
			}

		// Exit the loop and terminate all processes
		} else if (input.compare("quit") == 0) {
			break;

		} else {
		// Print help message
			std::cout << "Valid Commands:" << std::endl
								<< "list                print list of active child processes" << std::endl
								<< "bg COMMAND          executes external command COMMAND in background" << std::endl
								<< "fg PID              wait for previously running command in the background" << std::endl
								<< "signal PID SIGNAL   signal process PID the signal SIGNAL" << std::endl
								<< "stop PID            signal process PID to stop" << std::endl
								<< "continue PID        signal process PID to continue" << std::endl
								<< "quit                manually exit" << std::endl
								<< "help                print help message" << std::endl;
		}
	}
	std::cout << "Exiting..." << std::endl;
	// Terminate all started processes
	for (std::map<int, std::string>::iterator p = processes.begin(); p != processes.end(); ++p) {
		if (!sendSignal(p->first, SIGTERM)) {
			std::cout << "Error: Failure to terminate process " << p->first << std::endl;
		} else {
			std::cout << "DEBUG: Process " << p->first << " terminated" << std::endl;
		}
	}
	return EXIT_SUCCESS;
}
