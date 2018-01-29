# client.py
import sys
import zmq

# create socket and connect to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print('Receiving updates from message sending server...')
socket.connect('tcp://localhost:6000')

# receive/print in messages in a continous loop
while True:
    message = socket.recv_string()
    print(message)
