# client.py
import sys
import zmq

# create socket and connect to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print('Receiving updates from message sending server...')
socket.connect('tcp://localhost:6000')

# subscribe to the socket
socket_filter = sys.argv[1] if len(sys.argv) > 1 else '10001'
socket.setsockopt_string(zmq.SUBSCRIBE, socket_filter)

# ascii bytes to unicode
if isinstance(socket_filter, bytes):
    socket_filter = socket_filter.decode('ascii')
socket.setsockopt_string(zmq.SUBSCRIBE, socket_filter)

# receive/print in messages in a continous loop
while True:
    message = socket.recv_string()
    print(message)
