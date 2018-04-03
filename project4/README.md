# Project 4

## Contributors

Ann Keenan (akeenan2)

## Overview

This project incorporates producer and consumer threads to read from a pcap
file and check for redundancy between the packets. The number of threads
specified must be greater than 2, with a single producer and consumer thread at
the default configuration, and for every additional thread specified through
the command line, more consumer threads are added. The first level works by
matching to the entire packet contents, and the second level uses a window of
size 10 to find redundancy within the packets of 10 similar characters.

The hash produced was a uint32, such that there are 2^32, or over 4 billion
possible hashes. As such, storing the hashes into a vector any less than that
would allow for possible collisions. Collisions were dealt with by replacing
what was in the array before with the new packet, treating the new hash the
same way as if there was not already something in the array at that specific
index. This operation was determined on a random basis, with a 50-50 chance of
replacing the contents in the array.

Using an array of size 100 thousand, there was a significant chance of
encountering a collision, but this was a risk taken in order to preserve the
amount of space used for the array and performance with a O(1) lookup time.
Additionally, the effectiveness of the hash function was limited by the
modulus operation, but in order to preserve space, it was necessary. Each
PacketHash struct has a size of up to 1+32+2400+4 = 2437 bytes. An array of size
30,000 would therefore take up just over 64MB of space should each char array
be 2400 bytes in length.

## Performance

./threadedRE Dataset-Small.pcap Dataset-Small.pcap
Level 1, Number of threads 2.
Hits:         12782
Savings:      0.461811
Elapsed time: 0.130000

./threadedRE Dataset-Medium.pcap
Level 1, Number of threads 2.
Hits:         5591
Savings:      0.162190
Elapsed time: 0.250000
