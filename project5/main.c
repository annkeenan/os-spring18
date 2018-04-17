/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct frameEntry {
	int page;
	int bits;
};

int npages;
int nframes;
char *virtmem;
char *physmem;
struct disk *disk;

int *frameArr;
struct frameEntry *frameTable;
int *pageUsage;
int currFrame;

int diskReads;
int diskWrites;
int pageFaults;

void rand_fault_handler( struct page_table *pt, int page );
void fifo_fault_handler( struct page_table *pt, int page );
void lru_fault_handler( struct page_table *pt, int page );
int find_empty( struct page_table *pt );
void remove_page( struct page_table *pt, int frame );

// helper function for debugging
void print_table() {
	int i;
	for(i = 0; i < nframes; i++) {
		printf("%d\t%d\n", frameTable[i].page, frameTable[i].bits);
	}
	printf("\n");
}

int main( int argc, char *argv[] )
{
	// initialize random seed
	srand(time(NULL));

	if(argc!=5) {
		printf("Usage: ./virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	// set command line input
	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	if(!npages || !nframes) {
		fprintf(stderr, "Error: Invalid number of pages or frames\n");
		exit(1);
	} else if (npages < nframes) {
		fprintf(stderr, "Error: More frames (%d) than pages (%d). Defaulting to %d frames\n", nframes, npages, npages);
		nframes = npages;
	}
	const char *algorithm = argv[3];
	const char *program = argv[4];

	// create disk
	disk = disk_open("myvirtualdisk", npages);
	if(!disk) {
		fprintf(stderr, "Error: Couldn't create virtual disk: %s\n", strerror(errno));
		exit(1);
	}

	// create page table
	struct page_table *pt;
	if(strcmp(algorithm, "rand") == 0) {
		pt = page_table_create( npages, nframes, rand_fault_handler );
	} else if(strcmp(algorithm, "fifo") == 0) {
		pt = page_table_create( npages, nframes, fifo_fault_handler );
	} else if(strcmp(algorithm, "custom") == 0) { // lru algorithm
		pt = page_table_create( npages, nframes, lru_fault_handler );
	} else {
		fprintf(stderr, "Error: Unsupported algorithm: %s. Defaulting to rand\n", algorithm);
		pt = page_table_create( npages, nframes, rand_fault_handler );
	}
	if(!pt) {
		fprintf(stderr, "Error: Couldn't create page table: %s\n", strerror(errno));
		return 1;
	}

	// initialize global variables
	diskReads = 0;
	diskWrites = 0;
	pageFaults = 0;
	currFrame = 0;

	// allocate memory of global variables
	frameArr = malloc(nframes*sizeof(int));
	frameTable = malloc(nframes*sizeof(struct frameEntry));
	pageUsage = malloc(nframes*sizeof(int));

	virtmem = page_table_get_virtmem(pt);
	physmem = page_table_get_physmem(pt);

	if(strcmp(program, "sort") == 0) {
		sort_program(virtmem, npages*PAGE_SIZE);
	} else if(strcmp(program, "scan") == 0) {
		scan_program(virtmem, npages*PAGE_SIZE);
	} else if(strcmp(program, "focus") == 0) {
		focus_program(virtmem, npages*PAGE_SIZE);
	} else {
		fprintf(stderr, "Error: unknown program: %s\n", argv[3]);
		return 1;
	}

	// clean up
	page_table_delete(pt);
	disk_close(disk);
	free(pageUsage);
	free(frameTable);
	free(frameArr);

	fprintf(stdout, "disk reads (%d), writes (%d), page faults (%d)\n", diskReads, diskWrites, pageFaults);

	return 0;
}

void rand_fault_handler( struct page_table *pt, int page )
{
	// printf("page fault on page %d\n", page);
	// print_table();
	int frame, bits;
	page_table_get_entry(pt, page, &frame, &bits);

	// not in table
	if(!bits) {
		bits = PROT_READ;
		frame = find_empty(pt);

		// all pages filled
		if(frame == -1) {
			// find a random page
			frame = rand() % nframes;
			remove_page(pt, frame);
		}

		// read from the disk
		disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
		diskReads++;
	// no write
	} else if(bits & PROT_READ) {
		bits = PROT_READ | PROT_WRITE;
	} else {
		fprintf(stderr, "Error: Access fault on page #%d\n", page);
		exit(1);
	}

	// add the new page to the frame table// add the new page to the frame table
	page_table_set_entry(pt, page, frame, bits);
	frameTable[frame].page = page;
	frameTable[frame].bits = bits;

	pageFaults++;
	// print_table();
}

void fifo_fault_handler( struct page_table *pt, int page )
{
	// printf("page fault on page %d\n", page);
	// print_table();
	int frame, bits;
	page_table_get_entry(pt, page, &frame, &bits);

	// not in table
	if(!bits) {
		bits = PROT_READ;
		frame = find_empty(pt);

		// all pages filled
		if(frame == -1) {
			frame = frameArr[currFrame];
			remove_page(pt, frame);
		}

		// read from the disk
		disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
		diskReads++;

		frameArr[currFrame] = frame;
	// no write
	} else if(bits & PROT_READ) {
		bits = PROT_READ | PROT_WRITE;
	} else {
		fprintf(stderr, "Error: Access fault on page #%d\n", page);
		exit(1);
	}

	// add the new page to the frame table
	page_table_set_entry(pt, page, frame, bits);
	frameTable[frame].page = page;
	frameTable[frame].bits = bits;

	pageFaults++;

	// move the frame pointer for the next access
	currFrame = (currFrame + 1) % nframes;
	// print_table();
}

void lru_fault_handler( struct page_table *pt, int page )
{
	// printf("page fault on page %d\n", page);
	// print_table();
	int frame, bits;
	page_table_get_entry(pt, page, &frame, &bits);

	// not in table
	if(!bits) {
		bits = PROT_READ;
		frame = find_empty(pt);

		// all pages filled
		if(frame == -1) {
			int i, max = 0;
			for(i = 0; i < nframes; i++) {
				if (pageUsage[i] > pageUsage[max]) {
					max = i;
				}
			}
			frame = max;
			remove_page(pt, frame);
		}

		// read from the disk
		disk_read(disk, page, &physmem[frame*PAGE_SIZE]);
		diskReads++;
	} else if(bits & PROT_READ) {
		bits = PROT_READ | PROT_WRITE;
	// no write
	} else {
		fprintf(stderr, "Error: Access fault on page #%d\n", page);
		exit(1);
	}

	// add the new page to the frame table
	page_table_set_entry(pt, page, frame, bits);
	frameTable[frame].page = page;
	frameTable[frame].bits = bits;

	pageUsage[frame] = 0;
	int i;
	for(i = 0; i < nframes; i++) {
		pageUsage[i] += 1;
	}

	pageFaults++;
	// print_table();
}

int find_empty( struct page_table *pt )
{
	// check frame table for an empty page
	int i;
	for(i = 0; i < nframes; i++) {
		if(frameTable[i].bits == 0) {
			return i;
		}
	}
	return -1;
}

// remove the page from the frame table
void remove_page( struct page_table *pt, int frame )
{
	if(frameTable[frame].bits & PROT_WRITE) {
		disk_write(disk, frameTable[frame].page, &physmem[frame*PAGE_SIZE]);
		diskWrites++;
	}
	// clean the bits
	page_table_set_entry(pt, frameTable[frame].page, frame, 0);
	frameTable[frame].bits = 0;
}
