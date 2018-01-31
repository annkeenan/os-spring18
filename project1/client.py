# client.py
# Ann Keenan (akeenan2)
# Connect to the server on PORT and print received messages
import sys
import zmq
import datetime

PORT = '6000'  # default port

if len(sys.argv) == 2:
    PORT = sys.argv[1]
elif len(sys.argv) > 2:  # invalid command line call
    print('''Usage: python client.py PORT''')
    exit(1)

# Create socket and connect to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print('Receiving updates from message sending server...')
socket.connect('tcp://localhost:%s' % PORT)

# Receive/print messages in a continous loop
while True:
    message = socket.recv_string()
    st = datetime.datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')
    print('%s: %s' % (st, message))
