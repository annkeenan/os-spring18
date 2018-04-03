// threadedRE.cpp
// Ann Keenan (akeenan2)

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <iostream>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include "SpookyV2.h"

// /afs/nd.edu/coursesp.18/cse/cse30341.01/support/project4/Dataset-Small.pcap
#define MIN_PACKET 128
#define MAX_PACKET 2400
#define ARR_SIZE 30000
#define WINDOW_SIZE 64

pthread_mutex_t dequeMutex = PTHREAD_MUTEX_INITIALIZER;  // packets object
pthread_mutex_t setMutex = PTHREAD_MUTEX_INITIALIZER;  // packetSet object
pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;  // counter for hits/redundancies found
pthread_cond_t packetCond = PTHREAD_COND_INITIALIZER;  // packets object

struct ThreadArgs {
  int id;
  int level;
};

struct PacketData {  // store in the producer/consumer queue
  char data[MAX_PACKET];
  size_t length;
};

struct PacketHash {
  char valid;  // 0 false, 1 true
  uint32 hash;
  char data[MAX_PACKET];
  size_t length;
  PacketHash() : valid(0) {}
};

std::vector<std::string> files;  // list of files
std::deque<PacketData> packets;  // producer/consumer queue
struct PacketHash packetSet[ARR_SIZE];  // used to check for redundancy

int numPackets = 0;  // track total number of packets processed
int redundancy = 0;  // track bytes of redundancy
int hits = 0;  // level 1 = number of repeat packets, level 2 = number of repeat strings
int dataProcessed = 0;  // track total number of bytes in packets
char DONE = 0;
char DEBUG = 0;  // 0 disabled, 1 enabled

void *producer(void *args);
void *consumer(void *args);

void show_help() {
  printf("Usage: threadedRE [options] [files]\n");
  printf("Options:\n");
  printf("-level <level>    Level to run the program on. (default=1)\n");
  printf("-thread <threads> The number of threads to run. (default=2)\n");
  printf("-debug            Run in debug mode. (default=off)\n");
  printf("-v                Verbose mode. (default=off)\n");
  printf("-h                Show this help text.\n");
  printf("\nExamples:\n");
  printf("./threadedRE -level 1 -thread 3 test.pcap\n");
  printf("./threadedRE -level 2 -thread 2 test.pcap test.pcap\n\n");
}

int main(int argc, char *argv[]) {
  // Initialize random seed
  srand (time(NULL));

  // Default configuration values
  int level = 1;
  int numThreads = 2;

  // For each command line argument
  for (int i=1; i<argc; i++) {
    if (strcmp(argv[i], "-h") == 0) {
    // Print help message
      show_help();
      return EXIT_SUCCESS;
    // Set the level, default 1
    } else if (strcmp(argv[i], "-level") == 0) {
      i++;
      if (strcmp(argv[i], "1") == 0 || strcmp(argv[i], "2") == 0) {
        level = atoi(argv[i]);
      } else {
        printf("Error: Invalid level %s. Defaulting to 1.\n", argv[i]);
      }
    // Set the number of threads, default 1
    } else if (strcmp(argv[i], "-thread") == 0) {
      i++;
      if (isdigit(argv[i][0]) && atoi(argv[i]) > 1) {
        numThreads = atoi(argv[i]);
      } else {
        printf("Error: '%s' NaN or less than 2. Defaulting to 2.\n", argv[1]);
      }
    // Set print modes
    } else if (strcmp(argv[i], "-debug") == 0) {
      DEBUG = 1;
    // Check to make sure no other illegal arguments
    } else if (strncmp(argv[i], "-", 1) == 0) {
        printf("ERROR: Illegal argument %s.\n", argv[i]);
    // Add file names to vector
    } else {
      files.push_back(argv[i]);
    }
  }

  printf("Level %d, Number of threads %d.\n", level, numThreads);

  // Set thread arguments
  ThreadArgs ptArgs, ctArgs[numThreads-1];
  ptArgs.id = 0;
  ptArgs.level = level;
  for (int i = 0; i < numThreads-1; i++) {
    ctArgs[i].id = i;
    ctArgs[i].level = level;
  }

  // Start the clock
  clock_t begin = clock();

  // Set threads to be joinable
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Create threads
  int rc;
  pthread_t p, c[numThreads-1];
  if ((rc = pthread_create(&p, NULL, producer, (void*) &ptArgs)) != 0) {
    printf("ERROR: Unable to create producer thread with exit code %d.\n", rc);
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < numThreads-1; i++) {
    if ((rc = pthread_create(&c[i], NULL, consumer, (void*) &ctArgs[i])) != 0) {
      printf("ERROR: Unable to create consumer thread %d with exit code %d.\n", i, rc);
      exit(EXIT_FAILURE);
    }
  }

  // Join threads
  if ((rc = pthread_join(p, NULL)) != 0) {
    printf("ERROR: Unable to join producer thread with exit code %d.\n", rc);
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < numThreads-1; i++) {
    if ((rc = pthread_join(c[i], NULL)) != 0) {
      printf("ERROR: Unable to join consumer thread %d with exit code %d.\n", i, rc);
      exit(EXIT_FAILURE);
    }
  }
  pthread_attr_destroy(&attr);

  // Stop the clock
  clock_t end = clock();
  double elapsedTime = double(end - begin) / CLOCKS_PER_SEC;

  // Print statistics
  printf("%.2f MB processed\n", dataProcessed*1e-6);
  printf("%d hits\n", hits);
  printf("%.2f%% redundancy detected\n", (float)redundancy/(float)dataProcessed * 100);
  printf("%.2fs time elapsed\n", elapsedTime);

  return 0;
}

// Producer thread to read from file
void *producer(void *args) {
  ThreadArgs *tArgs = (ThreadArgs *) args;
  for (std::vector<std::string>::iterator f = files.begin(); f!=files.end(); ++f) {
    // Check if file has correct extension
    if ((*f).compare((*f).size()-5, 5, ".pcap") != 0) {
      printf("ERROR: File %s is not a pcap file. Skipping.\n", (*f).c_str());
      continue;
    }

    size_t rval;  // store return values

    FILE *fp;
    fp = fopen((*f).c_str(), "r");
    if (fp == NULL) {
      printf("ERROR: File %s does not exist. Skipping.\n", (*f).c_str());
      continue;
    }

    // Read and display magic Number
    uint32_t 	magicNum;
    rval = fread(&magicNum, 4, 1, fp);
    if (rval != 4) {
      // printf("ERROR: Unable to read the magic number.\n");
    } else if (DEBUG) {
      printf("Magic number %X.\n", magicNum);
    }
    fseek(fp, 24, SEEK_CUR);  // skip over the rest of the global header

    // Read all packets and print lengths
    uint32_t pLength;
    char pData[MAX_PACKET];

    while(!feof(fp)) {
      fseek(fp, 8, SEEK_CUR);	 // skip ts_sec/ts_usec
      fread(&pLength, 4, 1, fp);  // read incl_len field
      fseek(fp, 4, SEEK_CUR);  // skip orig_len

      // Check if packet is too large
      if (pLength < 128) {
        // printf("Packet is too small. Skipping %d bytes ahead.\n", pLength);
        fseek(fp, pLength, SEEK_CUR);
      } else if (pLength > MAX_PACKET) {
        // printf("Packet is too big. Skipping %d bytes ahead.\n", pLength);
        fseek(fp, pLength, SEEK_CUR);
      } else {
      // Read the packet
        rval = fread(pData, 1, pLength, fp);

        // Check if an error occured
        if (rval != pLength && !feof(fp)) {
          printf("ERROR: Did not read full packet. Return value %zu.\n", rval);
        } else {
          numPackets++;
          dataProcessed += pLength;

          // Create new object to push into queue
          PacketData packetData;
          memcpy(packetData.data, &pData[52], pLength-51);
          packetData.length = pLength-51;

          // Add packet to global array
          pthread_mutex_lock(&dequeMutex);
          if (DEBUG) {
            printf("Producer thread %d acquired deque lock.\n", tArgs->id);
          }
          packets.push_back(packetData);
          pthread_mutex_unlock(&dequeMutex);
          if (DEBUG) {
            printf("Producer thread %d released deque lock.\n", tArgs->id);
          }

          // Send signal to the consumer that a new object has been added to the queue
          pthread_cond_signal(&packetCond);
          if (DEBUG) {
            printf("Producer thread %d signaled consumer thread.\n", tArgs->id);
          }
        }
      }
    }
    fclose(fp);
  }
  DONE = true;
  return NULL;
}

// Consumer thread to check for redundancy
void *consumer(void *args) {
  ThreadArgs *tArgs = (ThreadArgs *) args;
  SpookyHash sHash;

  // Loop until files are all read
  while (1) {
    pthread_mutex_lock(&dequeMutex);
    if (DEBUG) {
      printf("Consumer thread %d acquired deque lock.\n", tArgs->id);
    }

    // Check if a packet is in the deque
    while (packets.empty()) {
      if (DONE) {  // producer threads done reading files
        pthread_mutex_unlock(&dequeMutex);
        if (DEBUG) {
          printf("Consumer thread %d released deque lock.\n", tArgs->id);
        }
        return NULL;
      }

      // Wait until the producer adds to the deque
      if (DEBUG) {
        printf("Consumer thread %d released deque lock. Waiting on condition.\n", tArgs->id);
      }
      pthread_cond_wait(&packetCond, &dequeMutex);
      if (DEBUG) {
        printf("Consumer thread %d acquired deque lock.\n", tArgs->id);
      }
    }

    // Use the packet at the front of the deque
    // Create a new char array to store the packet data
    char packet[packets.front().length];
    int packetLen = packets.front().length;
    memcpy(packet, packets.front().data, packetLen);
    // Remove the packet at the front of the deque
    packets.pop_front();
    pthread_mutex_unlock(&dequeMutex);
    if (DEBUG) {
      printf("Consumer thread %d released deque lock.\n", tArgs->id);
    }

    // Level 1
    if (tArgs->level == 1) {
      // Calculate the hash of the packet
      uint32 hash = sHash.Hash32(packet, sizeof(packet), 0);

      // Check for redundancy
      int index = hash % ARR_SIZE;
      pthread_mutex_lock(&setMutex);
      if (DEBUG) {
        printf("Consumer thread %d acquired set lock.\n", tArgs->id);
      }
      // If the hash matches a previous hash and data matches the previous data
      if (packetSet[index].valid && packetSet[index].hash == hash &&
        memcmp(packet, packetSet[index].data, packetSet[index].length) == 0) {
        if (DEBUG) {
          printf("Redundancy found. Hash: %llu.\n", (long long) hash);
        }

        // Release set lock before waiting for counter lock
        pthread_mutex_unlock(&setMutex);
        if (DEBUG) {
          printf("Consumer thread %d released set lock.\n", tArgs->id);
        }

        // Increment the counter for the number of hits
        pthread_mutex_lock(&counterMutex);
        if (DEBUG) {
          printf("Consumer thread %d acquired counter lock.\n", tArgs->id);
        }
        hits++;
        redundancy+= packetSet[index].length;
        pthread_mutex_unlock(&counterMutex);
        if (DEBUG) {
          printf("Consumer thread %d released counter lock.\n", tArgs->id);
        }
      } else {
        // If collision, randomly determine if the space should be replaced
        if (packetSet[index].valid) {
          if (rand() % 2) {
            packetSet[index].hash = hash;
            memcpy(packetSet[index].data, packet, packetLen);
            packetSet[index].length = packetLen;
          }
          pthread_mutex_unlock(&setMutex);
          if (DEBUG) {
            printf("Consumer thread %d released set lock.\n", tArgs->id);
          }
        // If new hash, store in the array
        } else {
          packetSet[index].valid = 1;
          packetSet[index].hash = hash;
          memcpy(packetSet[index].data, packet, packetLen);
          packetSet[index].length = packetLen;
        }
        pthread_mutex_unlock(&setMutex);
        if (DEBUG) {
          printf("Consumer thread %d released set lock.\n", tArgs->id);
        }
      }
    // Level 2
    } else {
      int match = -1;  // track the starting position of a matching string
      for (size_t i = 0; i < sizeof(packet)-WINDOW_SIZE; i++) {
        char buf[WINDOW_SIZE];
        memcpy(buf, &packet[i], WINDOW_SIZE);
        // Calculate the hash of the packet window
        uint32 hash = sHash.Hash32(buf, sizeof(buf), 0);

        // Check for redundancy
        int index = hash % ARR_SIZE;
        pthread_mutex_lock(&setMutex);
        if (DEBUG) {
          printf("Consumer thread %d acquired set lock.\n", tArgs->id);
        }
        if (packetSet[index].valid && packetSet[index].hash == hash &&
          memcmp(packet, packetSet[index].data, packetSet[index].length) == 0) {
          if (DEBUG) {
            printf("Redundancy found at index pos %d. Hash: %llu.\n", index, (long long) hash);
          }
          // If not currently tracking a redundancy, set to the current index position
          if (match == -1) {
            match = i;
          }
          // Release set lock before waiting for counter lock
          pthread_mutex_unlock(&setMutex);
          if (DEBUG) {
            printf("Consumer thread %d released set lock.\n", tArgs->id);
          }
        } else {
          // If collision, randomly determine if the space should be replaced
          if (packetSet[index].valid) {
            if (rand() % 2) {
              packetSet[index].hash = hash;
              memcpy(packetSet[index].data, buf, sizeof(buf));
              packetSet[index].length = sizeof(buf);
            }
          // If new hash, store in the array
          } else {
            packetSet[index].valid = 1;
            packetSet[index].hash = hash;
            memcpy(packetSet[index].data, buf, sizeof(buf));
            packetSet[index].length = sizeof(buf);
          }
          // Release set lock before waiting for counter lock
          pthread_mutex_unlock(&setMutex);
          if (DEBUG) {
            printf("Consumer thread %d released set lock.\n", tArgs->id);
          }

          // If a match was found before, add the redundancy
          if (match != -1) {
            pthread_mutex_lock(&counterMutex);
            if (DEBUG) {
              printf("Consumer thread %d acquired counter lock.\n", tArgs->id);
            }
            // Add the bytes from the first to last matched place
            redundancy += (i + WINDOW_SIZE - 1) - match;
            // Add a hit
            hits += 1;
            pthread_mutex_unlock(&counterMutex);
            if (DEBUG) {
              printf("Consumer thread %d released counter lock.\n", tArgs->id);
            }
            match = -1; // reset the tracker
            i += WINDOW_SIZE;  // move the iterator past the matched string
          }
        }
      }
    }
  }
  return NULL;
}
