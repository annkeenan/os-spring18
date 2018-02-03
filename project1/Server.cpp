// Server.cpp
// Ann Keenan (akeenan2)
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <zmq.hpp>
#include <sys/stat.h>

// Set up the ZMQ Socket
zmq::context_t context(1);
zmq::socket_t socket(context, ZMQ_PUB);

// Cleanup the ZMQ Socket
void cleanup() {
	// Call the functions called by the destructors of the socket/context
	if (!zmq_close(&socket) || !zmq_term(&context)) {
		std::cout << "Error: cleanup failed" << std::endl;
	}
}

// Handler for SIGINT
void sigintHandler(int signum) {
	std::cout << "Thanks for using! Exiting..." << std::endl;
	cleanup();
	exit(signum);
}

// Check if the input is a positive integer
bool isValid(const std::string & s) {
	if (s.empty() || (!isdigit(s[0]) && s[0] == '-')) return false;
	char *p;
	strtol(s.c_str(), &p, 10);
	return (*p == 0);
}

// Display help message
void helpMessage() {
  std::cout << "The commands are as follows:" << std::endl
            << "  help        This command" << std::endl
            << "  send XXX    Send the file XXX out to all subscribed clients" << std::endl
            << "  quit        Exit this code" << std::endl;
}

int main(int argc, char *argv[]) {
	// Catch Control-C via the signal handler
	signal(SIGINT, sigintHandler);

	// Check if port was specified
  if (argc != 2) {
    std::cout << "Error: Unable to launch - port not specified!" << std::endl
              << "Proper Usage:  ./project1   XXXX   where XXXX is the port number"
              << std::endl;
		return 1;
  }
	std::cout << "Welcome! Type 'help' for a list of possible commands." << std::endl;

	// Check if port is an integer
	if (!isValid(argv[1])) {
		std::cout << "Error: Invalid port number " << argv[1] << std::endl;
		return 1;
	}

	// Build address and bind the socket
	std::string address = "tcp://*:";
	address += argv[1];
	socket.bind(address.c_str());

	// Main loop
	while (1) {
		zmq::message_t request;

		// Prompt for user input
		std::string input;
		std::cout << "> ";
		std::cin >> input;

		// Parse what the user typed
		if (input.compare("help") == 0) {
			helpMessage();
			continue;  // Loop again
		} else if (input.compare("quit") == 0) {
			break;  // Exit the loop
		} else if (input.compare("send") == 0){
			// User input of the file name
			std::string filename;
			std::cin >> filename;

			// Check if file exists/permissions correct
			struct stat results;
			if (stat(filename.c_str(), &results) == -1) {
				std::cout << "Error: There is no file named " << filename << std::endl;
				continue;
			} else if (!(results.st_mode & S_IRUSR)) {
				std::cout << "Error: Permission denied to read file " << filename << std::endl;
				continue;
			}

			// Try to read the file
			std::ifstream f(filename);
			char message[120] = "\0"; // store the message to be sent
			int numChar = 0; // number of characters in file

			// Open the file and read contents
			char c;
			bool invalidInput = false;
			while (f.get(c) && numChar < 120) {  // Read 120 characters maximum
				// Check if the character is ASCII readable
				if ((int(c) >= 32 && int(c) <= 126) || c == '\n') {
					message[numChar++] = c;
				} else {
					invalidInput = true;
					break;
				}
			}

			// Check if all characters were ASCII readable
			if (invalidInput) {
				std::cout << "Error: The file contains non-ASCII readable text" << std::endl;
				continue;
			}

			// Handle files that are too long
			if (numChar > 120) {
				std::cout << "Error: The file length exceeds 120 characters" << std::endl;
				continue;
			} else if (numChar == 0) {
				std::cout << "Error: The file is empty" << std::endl;
				continue;
			}

			// Confirm message to be sent
			std::cout << "The following message will be sent (" << numChar << " characters):"
								<< std::endl << message << std::endl
								<< "Type YES to confirm: ";
			std::string confirm;
			std::cin >> confirm;

			if (confirm.compare("YES") == 0) {
				// Send message to the client
        zmq::message_t output(numChar);
        memcpy(output.data(), message, numChar);

				// ZMQ sending mechanism
        if (socket.send(output)) {
					std::cout << "Message sent!" << std::endl;
					continue;
				}
			}

			// If sending not confirmed, or message failure
			std::cout << "Message not sent." << std::endl;
		} else {
			std::cout << "Invalid command. Type help for a list of valid commands." << std::endl;
		}
	}

	std::cout << "Thanks for using MP-MBS! Exiting..." << std::endl;
	cleanup();
	return 0;
}
