# client.py
# Ann Keenan (akeenan2)
import sys
import zmq

PORT = '6000'

# Create socket and connect to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print('Receiving updates from message sending server...')
socket.connect('tcp://localhost:%s' % PORT)

# Receive/print in messages in a continous loop
while True:
    message = socket.recv_string()
    print(message)
