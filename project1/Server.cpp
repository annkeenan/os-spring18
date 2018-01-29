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

// Handler for SIGINT
void sigintHandler(int signum) {
	std::cout << "Thanks for using MP-MBS! Exiting..." << std::endl;
	//zmq_close(socket);
	//zmq_ctx_destroy(context);
	exit(signum);
}

// Display help message
void helpMessage() {
  std::cout << "The commands for MP-MBS are as follows:" << std::endl
            << "  help        This command" << std::endl
            << "  send XXX    Send the file XXX out to all subscribed clients" << std::endl
            << "  quit        Exit this code" << std::endl;
}

int main(int argc, char *argv[]) {
	// Catch Control-C via the signal handler
	signal(SIGINT, sigintHandler);

	// Check if input is correct
  if (argc != 2) {
    std::cout << "Error: Unable to launch - port not specified!" << std::endl
              << "Proper Usage:  ./project1   XXXX   where XXXX is the port number"
              << std::endl;
		return 1;
  }

	// Bind the socket
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
			continue;
		} else if (input.compare("quit") == 0) {
			break;
		} else if (input.compare("send") == 0){
			// Input the file name
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
			char message[120]; // store the message to be sent
			int numChar = 0; // number of characters in file

			// Open the file and read contents
			char c;
			while (f.get(c) && numChar < 120) {
				// Check if the character is ASCII readable
				if (int(c) >= 32 && int(c) <= 126) {
					message[numChar++] = c;
				} else {
					std::cout << "Error: The file contains non-ASCII readable text" << std::endl;
					break;
				}
			}

			// Handle files that are too long
			if (numChar > 120) {
				std::cout << "Error: The file length exceeds 120 characters" << std::endl;
				continue;
			}

			// Confirm message to be sent
			std::cout << "The following message will be sent (" << numChar << " characters):"
								<< std::endl << message << std::endl
								<< "Type YES to confirm: ";
			std::string confirm;
			std::cin >> confirm;

			if (confirm.compare("YES") == 0) {
				// ZMQ sending mechanism
				std::cout << "Message sent!" << std::endl;

				// Send message to the client
        zmq::message_t reply(numChar);
        memcpy(reply.data(), message, numChar);
        socket.send(reply);
			} else {
				std::cout << "Message not sent." << std::endl;
			}
		} else {
			std::cout << "Invalid command. Type help for a list of valid commands." << std::endl;
		}
	}

	std::cout << "Thanks for using MP-MBS! Exiting..." << std::endl;
	//zmq_close(socket);
	//zmq_ctx_destroy(context);
	return 0;
}
