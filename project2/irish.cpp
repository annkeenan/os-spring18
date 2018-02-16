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

int LEVEL = 1;
std::string PORT = "6500";

std::map<int, std::string> processes;

// Set up the ZMQ Socket
zmq::context_t context(1);
zmq::socket_t socket(context, ZMQ_REP);
std::string message;  // return message

// Return pid of completed process
int waitProcesses() {
	int status, wpid;

	// Loop through list of started child processes
	for (std::map<int, std::string>::iterator p = processes.begin(); p != processes.end(); ++p) {
		// Check if the process is already completed
		if (p->second == "Terminated" || p->second == "Completed") {
			continue;
		}

		wpid = waitpid(p->first, &status, WNOHANG);
		if (wpid == 0) {  // process not complete
			continue;
		} else {
			if (wpid == -1) {  // error
				std::cout << "Error: Failure waiting for process " << p->first << std::endl;
				break;
			} else if (wpid != 0) {  // success in waiting
				// std::cout << "DEBUG: Process " << p->first << " completed" << std::endl;
				p->second = "Completed";
			}
			return p->first;
		}
	}

	// Return error code
	return -1;
}

void sigintHandler(int signum) {
	std::cout << "Exiting..." << std::endl;

	// Clean up all started child processes
	wait(NULL);

	// Call the functions called by the destructors of the socket/context
	if (!zmq_close(&socket) || !zmq_term(&context)) {
		std::cout << "Error: cleanup failed" << std::endl;
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

	// Change back to old status in case of error
	std::string oldStatus;
	oldStatus = p->second;

	if (sig == "SIGHUP") {  // hangup
		signum = SIGHUP;
		p->second = "Terminated";
	} else if (sig == "SIGALRM") {  // terminate when timer expires
		signum = SIGALRM;
		p->second = "Terminated";
	} else if (sig == "SIGINT") {
		signum = SIGINT;
		p->second = "Terminated";
	} else if (sig == "SIGQUIT") {  // quit and dump core
		signum = SIGQUIT;
		p->second = "Terminated";
	} else if (sig =="SIGTERM") {  // terminate gracefully
		signum = SIGTERM;
		p->second = "Terminated";
	} else if (sig == "SIGKILL") {  // terminate immediately
		signum = SIGKILL;
		p->second = "Terminated";
	} else if (sig == "SIGSTOP") {  // stop running
		if (p->second == "stopped") {
			std::cout << "Process " << pid << " already stopped" << std::endl;
			return 1;
		}
		signum = SIGSTOP;
		p->second = "Stopped";
	} else if (sig == "SIGCONT") {
		if (p->second == "Running") {
			message = "Process " + std::to_string(pid) + " already running\n";
			// std::cout << message;
			return 1;
		}
		p->second = "Running";
		signum = SIGCONT;
	} else {
		message = "Error: Signal " + sig + " is not supported\n";
		// std::cout << message;
		return 1;  // unsupported signal
	}

	if (!sendSignal(pid, signum)) {
		message = "Error: Failure to send signal " + sig + " on process " + std::to_string(pid) + "\n";
		// std::cout << message;
		p->second = oldStatus;
		return -1;
	} else {
		if (p->second.compare("terminated") == 0) {
			// std::cout << "DEBUG: Process " << p->first << " terminated" << std::endl;
			// processes.erase(p);
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
		message = "Process " + std::to_string(pid) + " started in background\n";
		// std::cout << message;
		processes.insert(std::pair<int, std::string>{pid, "Running"});
  } else {
    execvp(args.data()[0], args.data());
		std::cout << "Error: Failure starting command" << command << std::endl;
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

	if (LEVEL == 1) {
		std::cout << "Welcome to irish." << std::endl;
	} else {
		std::string addr;
		addr = "tcp://*:" + PORT;
		socket.bind(addr);
		std::cout << "Starting irish server on port " << PORT << "..." << std::endl;
	}

	// Main loop
	while (1) {
		// Prompt for user input
		std::string input, rpl;

		if (LEVEL == 1) {
			std::cout << "> ";
			std::cin >> input;
		} else {
			zmq::message_t reply;
			socket.recv(&reply);
			rpl = std::string(static_cast<char*>(reply.data()), reply.size());
			int index = rpl.find(" ");
			input = rpl.substr(0, index);
			rpl = rpl.substr(index + 1, rpl.length() - index);
			std::cout << "> " << input << std::endl;
			std::cout << rpl << std::endl;
		}

		// Wait and cleanup any completed processes
		int pid;
		while ((pid = waitProcesses()) != -1) {
			processes[pid] = "Completed";
		}

		// Start a process in the background
		if (input.compare("bg") == 0) {
			std::string line;

			if (LEVEL == 1) {
		  	std::getline(std::cin, line);
			} else {
				line = rpl;
			}

			// Execute the command
			exec(line);
		} else if (input.compare("list") == 0) {
			if (processes.empty()) {
				message = "No subprocesses.\n";
			} else {
				message = std::to_string(processes.size()) + " Background Process(es).\n";
				int n = 1;
				for (std::map<int, std::string>::iterator p=processes.begin(); p!=processes.end(); ++p) {
	        message += "(" + std::to_string(n) + ") PID=" + std::to_string(p->first) + " State=" + p->second + "\n";
					n++;
				}
			}
			// std::cout << message;

		// Bring process to foreground and wait to complete
		} else if (input.compare("fg") == 0) {
			int pid, status, wpid;
			if (LEVEL == 1) {
		  	std::cin >> pid;
			} else {
				pid = std::stoi(rpl);
			}
			std::map<int, std::string>::iterator p;

			// Process is either completed or does not exist
			if ((p = processes.find(pid)) == processes.end()) {
				std::cout << "Error: Process " << pid << " does not exist" << std::endl;
				continue;
			} else if (p->second == "Completed") {
				message = "Process " + std::to_string(pid) + " completed\n";
				// std::cout << message;
				// processes.erase(p);
				continue;
			} else if (p->second == "Stopped") {
				message = "Process " + std::to_string(pid) + " stopped\n";
				// std::cout << message;
				// processes.erase(p);
				continue;
			} else if (p->second == "Terminated") {
				message = "Process " + std::to_string(pid) + " terminated\n";
				// std::cout << message;
				// processes.erase(p);
				continue;
			}

			// Block until the running process is complete
	    wpid = waitpid(pid, &status, 0);
			if (wpid == -1) {
				std::cout << "Error: Failure to wait on process " << pid << std::endl;
				continue;
			}
			// std::cout << "DEBUG: Process " << pid << " complete" << std::endl;

		// Send signal to a process
		} else if (input.compare("signal") == 0) {
			int pid;
			std::string sig;
			if (LEVEL == 1) {
		  	std::cin >> pid >> sig;
			} else {
				int index = rpl.find(" ");
				pid = std::stoi(rpl.substr(0, index));
				sig = rpl.substr(index + 1, rpl.length() - index);
			}
			handleSignal(pid, sig);

		// Stop a process
		} else if (input.compare("stop") == 0) {
			int pid;
			if (LEVEL == 1) {
		  	std::cin >> pid;
			} else {
				pid = std::stoi(rpl);
			}

			if (sendSignal(pid, SIGSTOP)) {
				processes[pid] = "stopped";
				message = "Process " + std::to_string(pid) + " stopped.\n";
				// std::cout << message;
			}

		// Start a process again
		} else if (input.compare("continue") == 0) {
			int pid;
			if (LEVEL == 1) {
		  	std::cin >> pid;
			} else {
				pid = std::stoi(rpl);
			}

			if (sendSignal(pid, SIGCONT)) {
				processes[pid] = "Running";
				message = "Process " + std::to_string(pid) + " continued.\n";
				// std::cout << message;
			}

		// Exit the loop and terminate all processes
		} else if (input.compare("quit") == 0) {
			break;

		// Print help message
		} else {
			message = "Valid Commands:\nlist                print list of active child processes\nbg COMMAND          executes external command COMMAND in background\nfg PID              wait for previously running command in the background\nsignal PID SIGNAL   signal process PID the signal SIGNAL\nstop PID            signal process PID to stop\ncontinue PID        signal process PID to continue\nquit                manually exit\nhelp                print help message\n";
			// std::cout << message;
		}

		if (LEVEL == 1) {
			std::cout << message;
		} else {
			zmq::message_t reply(message.size());
			memcpy((void *) reply.data(), message.c_str(), message.size());
			socket.send(reply);
		}
	}
	std::cout << "Exiting..." << std::endl;

	// Terminate all started processes
	for (std::map<int, std::string>::iterator p = processes.begin(); p != processes.end(); ++p) {
		// Check if the process is already completed
		if (p->second == "Terminated" || p->second == "Completed") {
			continue;
		}

		// Stop processes if not complete
		if (!sendSignal(p->first, SIGTERM)) {
			std::cout << "Error: Failure to terminate process " << p->first << std::endl;
		} else {
			// std::cout << "DEBUG: Process " << p->first << " terminated" << std::endl;
		}
	}

	// Call the functions called by the destructors of the socket/context
	if (!zmq_close(&socket) || !zmq_term(&context)) {
		std::cout << "Error: cleanup failed" << std::endl;
	}

	return EXIT_SUCCESS;
}
