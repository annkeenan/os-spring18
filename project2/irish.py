#!/afs/nd.edu/user14/csesoft/2017-fall/anaconda3/bin/python3.6
# -*- coding: UTF-8 -*-

# irish.py
# Ann Keenan (akeenan2)
# Connect to the server on PORT and print received messages
import sys
import zmq
import datetime
import time

SERVER = 'student00.cse.nd.edu'
PORT = '6500'  # default port

if len(sys.argv) == 2:
    SERVER = sys.argv[1]
elif len(sys.argv) == 3:
    PORT = sys.argv[2]
elif len(sys.argv) > 2:  # invalid command line call
    print('''Usage: python3 irish.py [SERVER] [PORT]''')
    exit(1)

# Create socket and connect to server
print('Attempting to connect to %s on port %s...' % (SERVER, PORT))
context = zmq.Context()
socket = context.socket(zmq.REQ)
socket.connect('tcp://%s:%s' % (SERVER, PORT))
print('Connected.\n\nWelcome to irish.')

# Send commands and output the received messages
while True:
    command = input("> ")
    socket.send_string(command)

    if command == "quit":
        break

    message = socket.recv_string()
    print('%s' % message, end='')
