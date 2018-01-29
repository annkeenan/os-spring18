# client.py
import sys
import zmq

PORT = '6000'

# create socket and connect to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print('Receiving updates from message sending server...')
socket.connect('tcp://localhost:%s' % PORT)

# receive/print in messages in a continous loop
while True:
    message = socket.recv_string()
    print(message)