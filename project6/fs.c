
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int *BLOCK_MAP;
int IS_MOUNTED = 0;

struct fs_superblock {
  int magic;
  int nblocks;
  int ninodeblocks;
  int ninodes;
};

struct fs_inode {
  int isvalid;
  int size;
  int direct[POINTERS_PER_INODE];
  int indirect;
};

union fs_block {
  struct fs_superblock super;
  struct fs_inode inode[INODES_PER_BLOCK];
  int pointers[POINTERS_PER_BLOCK];
  char data[DISK_BLOCK_SIZE];
};

// Create a new filesystem
int fs_format()
{
  if(IS_MOUNTED == 1) {
    fprintf(stderr, "ERROR: Disk already mounted.\n");
    return 0;
  }

  // Initialize blank block
  union fs_block block;
  disk_read(0, block.data);

  int num_blocks = block.super.nblocks;
  int num_inode_blocks = block.super.ninodeblocks;

  // Clear inode table
  int i, j, k;
  for(k = 1; k <= num_inode_blocks; k++) {
    disk_read(k, block.data);
    for(i = 0; i < INODES_PER_BLOCK; i++) {
      block.inode[i].isvalid = 0;
      block.inode[i].indirect = 0;
      for(j = 0; j < POINTERS_PER_INODE; j++) {
        block.inode[i].direct[j] = 0;
      }
    }
    disk_write(k, block.data);
  }

  // Free remaining data blocks
  for(k = num_inode_blocks + 1; k < num_blocks; k++) {
    disk_read(k, block.data);
    memset(&block.data[0], 0, sizeof(block.data));
    disk_write(k, block.data);
  }

  // Initialize blank superblock
  struct fs_superblock new_superblock;
  new_superblock.magic = FS_MAGIC;
  new_superblock.nblocks = disk_size();

  // Sets aside 10% of the blocks for inodes
  new_superblock.ninodeblocks = new_superblock.nblocks * .10 + 1;
  new_superblock.ninodes = INODES_PER_BLOCK;  // * new_superblock.ninodeblocks;

  // Write superblock
  block.super = new_superblock;
  disk_write(0, block.data);

  return 1;
}

// Scan filesystem and report organization
void fs_debug()
{
	union fs_block block;
	union fs_block indirect_block;

	disk_read(0, block.data);

	// Store superblock variables
	int num_blocks = block.super.nblocks;
	int num_inode_blocks = block.super.ninodeblocks;
	int num_inodes = block.super.ninodes;

	printf("superblock:\n");
	printf("    magic number is %s \n", ((block.super.magic == FS_MAGIC) ? "valid" : "invalid"));
	printf("    %d blocks\n", num_blocks);
	printf("    %d inode blocks\n", num_inode_blocks);
	printf("    %d inodes\n", num_inodes);

	int i, j, k, remaining_size;
	for(k = 1; k <= num_inode_blocks; k++) {
		disk_read(k, block.data);
		for(i = 0; i < INODES_PER_BLOCK; i++) {
			// Print out valid inode blocks
			if(block.inode[i].isvalid) {
				printf("inode %d:\n", i);
				printf("    size %d bytes\n", block.inode[i].size);
				printf("    direct blocks:");
				for(j = 0; j < POINTERS_PER_INODE; j++) {
					if(block.inode[i].direct[j]) {
						printf(" %d", block.inode[i].direct[j]);
					}
				}
				printf("\n");

				// Print out valid indirect blocks
				if(block.inode[i].size > POINTERS_PER_INODE*DISK_BLOCK_SIZE){
					remaining_size = block.inode[i].size - POINTERS_PER_INODE*DISK_BLOCK_SIZE;
					printf("    indirect block: %d\n", block.inode[i].indirect);
					printf("    indirect data blocks:");
					disk_read(block.inode[i].indirect, indirect_block.data);
					for(j = 0; j < ceil(remaining_size/DISK_BLOCK_SIZE); j++) {
						if(indirect_block.pointers[j]) {
							printf(" %d", indirect_block.pointers[j]);
						}
					}
					printf("\n");
				}
			}
		}
	}
}

// Prepare filesystem for use
int fs_mount()
{
	union fs_block block;
	disk_read(0, block.data);

  // Error checking
	if(block.super.magic != FS_MAGIC) {
		fprintf(stderr, "ERROR: Magic number is invalid.\n");
		return 0;
	} else if(IS_MOUNTED) {
		fprintf(stderr, "ERROR: Disk already mounted.\n");
		return 0;
	}

  // Store the superblock variables
  int num_blocks = block.super.nblocks;
  int num_inode_blocks = block.super.ninodeblocks;

	// Initialize and fill the bitmap with zeros
	int i;
	BLOCK_MAP = malloc(sizeof(int)*num_blocks);
	for(i = 0; i < num_blocks; i++) {
		BLOCK_MAP[i] = 0;
	}
	BLOCK_MAP[0] = 1;  // superblock

  // Update unavailable positions with 1s
  int j, k;
	double remaining_size;
  for(k = 1; k <= num_inode_blocks; k++) {
    BLOCK_MAP[k] = 1;  // mark inode blocks
    disk_read(k, block.data);
    for(i = 0; i < INODES_PER_BLOCK; i++) {
      if(block.inode[i].isvalid) {
        // Direct blocks
        for(j = 0; j < POINTERS_PER_INODE; j++) {
          if(block.inode[i].direct[j]) {
            BLOCK_MAP[block.inode[i].direct[j]] = 1;
          }
        }
        // Indirect blocks
        if(block.inode[i].size > POINTERS_PER_INODE*DISK_BLOCK_SIZE) {
          remaining_size = block.inode[i].size - POINTERS_PER_INODE*DISK_BLOCK_SIZE;
          BLOCK_MAP[block.inode[i].direct[j]] = 1;
          disk_read(block.inode[i].indirect, block.data);
          for(j = 0; j < ceil(remaining_size/DISK_BLOCK_SIZE); j++) {
            BLOCK_MAP[block.pointers[j]] = 1;
          }
          disk_read(k, block.data);
        }
      }
    }
  }

  IS_MOUNTED = 1;
  return 1;
}

// Create new inode
int fs_create()
{
	// Error checking
	if(!IS_MOUNTED) {
		fprintf(stderr, "ERROR: File system not mounted.\n");
		return 0;
	}

	// Get number of inodes
	union fs_block block;
	disk_read(0, block.data);
	if(block.super.magic != FS_MAGIC) {
		fprintf(stderr, "ERROR: Magic number is invalid.\n");
		return 0;
	}

  int num_inode_blocks = block.super.ninodeblocks;

  // Create new inode
  struct fs_inode new_inode;
  new_inode.isvalid = 1;
  new_inode.size = 0;
  int i;
  for(i = 0; i < POINTERS_PER_INODE; i++) {
    new_inode.direct[i] = 0;
  }
  new_inode.indirect = 0;

  // Find an empty inode block
  int k;
  for(k = 1; k <= num_inode_blocks; k++) {
    disk_read(k, block.data);
    int start;

    // Skip zero index of the first block
		if (k == 1) {
			start = 1;
		} else {
			start = 0;
		}

		// If inode is not yet created
		for(i = start; i < INODES_PER_BLOCK; i++) {
			if(!block.inode[i].isvalid) {
				block.inode[i] = new_inode;
				disk_write(k, block.data);
				return i + INODES_PER_BLOCK*(k-1);
			}
		}
  }

  // Exiting loop means it couldn't find an open inode
  printf("Error: Unable to create new inode. No spaces available.\n");
  return 0;
}

// Delete inode
int fs_delete( int inumber )
{
	if(!IS_MOUNTED) {
		fprintf(stderr, "ERROR: File system not mounted.\n");
		return 0;
	}

  // Convert the numbers
  int block_number = inumber/INODES_PER_BLOCK + 1;
  int inode_number = inumber % INODES_PER_BLOCK;

  // Read the block
	union fs_block block;
	disk_read(block_number, block.data);

	// Check that block is valid
	if(!block.inode[inode_number].isvalid) {
		fprintf(stderr, "ERROR: Invalid inode (%d) to delete.\n", inode_number);
		return 0;
	}

  // Set all values to 0
  block.inode[inode_number].isvalid = 0;
	block.inode[inode_number].size = 0;
	int i;
	for(i = 0; i < POINTERS_PER_INODE; i++) {
		block.inode[inode_number].direct[i] = 0;
		BLOCK_MAP[block.inode[inode_number].direct[i]] = 0;
	}

  // Read indirect block and set to 0
	union fs_block indirect_block;
	disk_read(block.inode[inode_number].indirect, indirect_block.data);
	for(i = 0; i < POINTERS_PER_BLOCK; i++) {
		indirect_block.pointers[i] = 0;
		BLOCK_MAP[indirect_block.pointers[i]] = 0;
	}
	block.inode[inode_number].indirect = 0;

  // Set all values to 0
  disk_write(block_number, block.data);
  return 1;
}

// Return logical size of inode
int fs_getsize( int inumber )
{
  // Read the block
  union fs_block block;
  disk_read(0, block.data);

	// Store the superblock variables
	int num_inode_blocks = block.super.ninodeblocks;

	// Check if valid
	if (inumber < 1 || inumber > INODES_PER_BLOCK * num_inode_blocks) {
		fprintf(stderr, "ERROR: Invalid input number (%d).\n", inumber);
		return 0;
	}

	// Convert the numbers
	int block_number = inumber/INODES_PER_BLOCK + 1;
	int inode_number = inumber % INODES_PER_BLOCK;

	// Read the inode block
	disk_read(block_number, block.data);

	// Return size if valid block
	if(block.inode[inode_number].isvalid) {
		return block.inode[inode_number].size;
	}
	return -1;
}

// Read data from valid inode
int fs_read( int inumber, char *data, int length, int offset )
{
	if(!IS_MOUNTED) {
		fprintf(stderr, "ERROR: File system not mounted.\n");
		return 0;
	}

	// Convert the inumber
	int block_number = inumber/INODES_PER_BLOCK + 1;
	int inode_number = inumber % INODES_PER_BLOCK;

	// Read the block
	union fs_block block;
	disk_read(block_number, block.data);

	// Check if valid
	if (!block.inode[inode_number].isvalid) {
		fprintf(stderr, "ERROR: Invalid inode number (%d).\n", inumber);
		return 0;
	}

	// Calculate the number of direct pointers
	int inode_size = block.inode[inode_number].size;
  int num_direct_pointers = ceil((double)inode_size / DISK_BLOCK_SIZE);
  if (num_direct_pointers > POINTERS_PER_INODE) {
    num_direct_pointers = POINTERS_PER_INODE;
  }

	// Nothing to read
	if(offset >= inode_size) {
		return 0;
	}

	// Variables for reading through blocks
	int data_read = 0;
	int i, j;
	int indirect_block_number;
	int position = offset;

	// Read from direct blocks
	for(i = 0; i < num_direct_pointers; i++) {
		// Read in the direct block
		disk_read(block.inode[inode_number].direct[i], block.data);

		// If read less than one block
		if(length + offset < DISK_BLOCK_SIZE) {
			for(j = 0; j < length + offset; j++) {
				data[j-offset] = block.data[i];
			}
			return j-offset;
		}

		// Skip the block if offset greater than block size
		if(position >= DISK_BLOCK_SIZE) {
			position -= DISK_BLOCK_SIZE;
			disk_read(block_number, block.data);
			continue;
		}

		// If less than one block left to read
		if(inode_size - offset + data_read < DISK_BLOCK_SIZE) {
			for(j = 0; j < inode_size - offset + data_read; j++) {
				data[data_read+j] = block.data[position+j];
			}
			return data_read + j;
		}

		// If less than one block requested
		if(data_read + DISK_BLOCK_SIZE + position >= length) {
			for(j = 0; j < length - data_read; j++) {
				data[data_read+j] = block.data[position+j];
			}
			return length - data_read;
		}

		// Full read of block
		for(j = 0; j < DISK_BLOCK_SIZE - position; j++) {
			data[data_read+j] = block.data[position+j];
		}
		data_read += DISK_BLOCK_SIZE;

		// Prepare to read next block
		position = 0;
		disk_read(block_number, block.data);
	}

	// Calculate number of indirect pointers
	int num_indirect_pointers;
	num_indirect_pointers = ceil((double)inode_size / (double)DISK_BLOCK_SIZE) - num_direct_pointers;
	if (num_indirect_pointers == 0) {
		return data_read;
	}

	disk_read(block_number, block.data);
	indirect_block_number = block.inode[inode_number].indirect;
	disk_read(indirect_block_number, block.data);

	for(i = 0; i < num_indirect_pointers; i++) {
		disk_read(block.pointers[i], block.data);

		// Skip block if offset larger than the block size
		if(position >= DISK_BLOCK_SIZE) {
				position -= DISK_BLOCK_SIZE;
				disk_read(indirect_block_number, block.data);
				continue;
		}

		// If less than one block left to read
		if(inode_size - offset + data_read < DISK_BLOCK_SIZE) {
			for(j = 0; j < inode_size - offset + data_read; j++) {
				data[data_read+j] = block.data[position+j];
			}
			return data_read + j;
		}

		// If less than one block requested
		if(data_read + DISK_BLOCK_SIZE + position >= length) {
			for(j = 0; j < length - data_read; j++) {
				data[data_read+j] = block.data[position+j];
			}
			return length - data_read;
		}

		// Full read of block
		for(j = 0; j < DISK_BLOCK_SIZE - position; j++) {
			data[data_read+j] = block.data[position+j];
		}
		data_read += DISK_BLOCK_SIZE;

		// Prepare to read next block
		position = 0;
		disk_read(indirect_block_number, block.data);
	}

	return 0;
}

// Write data to valid inode
int fs_write( int inumber, const char *data, int length, int offset ) {
  if(!IS_MOUNTED) {
    fprintf(stderr, "ERROR: File system not mounted.\n");
    return 0;
  }

	// Read the block
	union fs_block block;
	disk_read(0, block.data);

  // Store the superblock variables
	int num_blocks = block.super.nblocks;
	int num_inode_blocks = block.super.ninodeblocks;

	// Check if valid
	if (inumber < 1 || inumber > INODES_PER_BLOCK * num_inode_blocks) {
		fprintf(stderr, "ERROR: Invalid input number (%d).\n", inumber);
		return 0;
	}

  // Convert the inumber
	int block_number = inumber/INODES_PER_BLOCK + 1;
	int inode_number = inumber % INODES_PER_BLOCK;

  // Read from desired inode
	disk_read(block_number, block.data);
	if (!block.inode[inode_number].isvalid) {
		fprintf(stderr, "ERROR: Invalid inode number (%d).\n", inode_number);
		return 0;
	}

  // Calculate the number of direct pointers
	int inode_size = block.inode[inode_number].size;
  int num_direct_pointers = ceil((double)inode_size / DISK_BLOCK_SIZE);
  if (num_direct_pointers > POINTERS_PER_INODE) {
    num_direct_pointers = POINTERS_PER_INODE;
  }

  // Variables for reading through blocks
	int data_written = 0;
	int i, j, k;
  int direct_block_number, indirect_block_number;
  int position = offset;

  // Skip direct blocks if offset is greater than number of bytes
  if(offset > POINTERS_PER_INODE*DISK_BLOCK_SIZE) {
    position -= POINTERS_PER_INODE*DISK_BLOCK_SIZE;
  // Decrement the written length by the size of the direct pointers
  } else {
    for(i = 0; i < POINTERS_PER_INODE; i++) {
      // If need to allocate a direct pointer
      if(i+1 > num_direct_pointers) {
        for(j = 0; j < num_blocks; j++) {
          // Write to block if free
          if(BLOCK_MAP[j] == 0) {
            BLOCK_MAP[j] = 1;
            block.inode[inode_number].direct[i] = j;
            disk_write(block_number, block.data);
            disk_read(block_number, block.data);
            break;
          }
        }
        // All blocks are full
        if(j == num_blocks) {
          // Update the size of the inode
          if(offset + data_written > inode_size) {
            block.inode[inode_number].size = offset + data_written;
            disk_write(block_number, block.data);
          }
          return data_written;
        }
      }
      // Skip the block
      if(position > DISK_BLOCK_SIZE) {
        position -= DISK_BLOCK_SIZE;
        continue;
      }
      // Read in the direct block
      direct_block_number = block.inode[inode_number].direct[i];
      disk_read(direct_block_number, block.data);

      // Write data
      for(k = position; k < DISK_BLOCK_SIZE; k++) {
        block.data[k] = data[data_written];
        data_written++;
        position--;

        // All data written
        if(data_written >= length) {
          disk_write(direct_block_number, block.data);
					disk_read(block_number, block.data);

          // Update inode size
          if((offset + data_written) > inode_size) {
            block.inode[inode_number].size = offset + data_written;
            disk_write(block_number, block.data);
          }
          return data_written;
        }
      }

      position = 0;
      disk_write(direct_block_number, block.data);
			disk_read(block_number, block.data);
    }
  }

	// Calculate number of indirect pointers
	int num_indirect_pointers = ceil((double)inode_size / (double)DISK_BLOCK_SIZE) - num_direct_pointers;
  disk_read(block_number, block.data);

	// If block already allocated
  if(num_indirect_pointers > 0){
      indirect_block_number = block.inode[inode_number].indirect;
  } else {
		// If need to allocate an indirect pointer
    for(j = 0; j < num_blocks; j++) {
			// Write to block if free
      if(BLOCK_MAP[j] == 0){
        BLOCK_MAP[j] = 1;
        indirect_block_number = j;
        block.inode[inode_number].indirect = j;
        disk_write(block_number, block.data);
        break;
      }
    }
		// All data blocks full
    if(j == num_blocks) {
			// Update inode size
      if((offset + data_written) > inode_size) {
        block.inode[inode_number].size = offset + data_written;
        disk_write(block_number, block.data);
       }
       return data_written;
    }
  }

	for(i = 0; i < POINTERS_PER_INODE; i++) {
		// If need to allocate an indirect pointer
		if(i+1 > num_indirect_pointers) {
			for(j = 0; j < num_blocks; j++) {
				// Write to block if free
				if(BLOCK_MAP[j] == 0) {
          BLOCK_MAP[j] = 1;
					disk_read(indirect_block_number, block.data);
          block.pointers[i] = j;
          disk_write(indirect_block_number, block.data);
					disk_read(block_number, block.data);
          break;
        }
      }
      if(j == num_blocks) {
        fprintf(stderr, "ERROR: All data blocks full.\n");
        if(offset + data_written > inode_size) {
          block.inode[inode_number].size = offset + data_written;
          disk_write(block_number, block.data);
        }
        return data_written;
      }
    }
    // Check offset
    if(position > DISK_BLOCK_SIZE) {
        position -= DISK_BLOCK_SIZE;
        continue;
    }

		disk_read(indirect_block_number, block.data);
    direct_block_number = block.pointers[i];
    disk_read(direct_block_number, block.data);

    // Write data
    for(k = position; k < DISK_BLOCK_SIZE; k++) {
      block.data[k] = data[data_written];
      data_written++;
      position--;

      // All data written
      if(data_written >= length) {
        disk_write(direct_block_number, block.data);
				disk_read(block_number, block.data);

        // Update inode size
        if((offset + data_written) > inode_size) {
          block.inode[inode_number].size = offset + data_written;
          disk_write(block_number, block.data);
        }
        return data_written;
      }
    }

    position = 0;
    disk_write(direct_block_number, block.data);
		disk_read(block_number, block.data);
  }

	disk_read(block_number, block.data);
  block.inode[inode_number].size = offset + data_written;
  disk_write(block_number, block.data);
  return data_written;
}
