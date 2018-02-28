// mandel.c
// Ann Keenan (akeenan2)

#include "bitmap.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  struct bitmap *bm;
  int max;
  double xmin;
  double xmax;
  double ymin;
  double ymax;
  int num_threads;
  int id;
} ThreadArgs;

int iteration_to_color(int, int);
int iterations_at_point(double, double, int);
void *compute_image(void *args);

void show_help() {
  printf("Use: mandel [options]\n");
  printf("Where options are:\n");
  printf("-m <max>     The maximum number of iterations per point. (default=1000)\n");
  printf("-n <threads> The number of threads to run. (default=1)\n");
 	printf("-x <coord>   X coordinate of image center point. (default=0)\n");
  printf("-y <coord>   Y coordinate of image center point. (default=0)\n");
  printf("-s <scale>   Scale of the image in Mandlebrot coordinates. (default=4)\n");
  printf("-W <pixels>  Width of the image in pixels. (default=500)\n");
  printf("-H <pixels>  Height of the image in pixels. (default=500)\n");
  printf("-o <file>    Set output file. (default=mandel.bmp)\n");
  printf("-h           Show this help text.\n");
  printf("\nSome examples are:\n");
  printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
  printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
  printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

int main(int argc, char *argv[]) {
  char c;

  // These are the default configuration values used
  // if no command line arguments are given.
  const char *outfile = "mandel.bmp";
  double xcenter = 0;
  double ycenter = 0;
  double scale = 4;
  int    image_width = 500;
  int    image_height = 500;
  int    max = 1000;
  int    num_threads = 1;

  // For each command line argument given,
  // override the appropriate configuration value.
  while ((c = getopt(argc, argv, "x:y:s:W:H:m:o:n:h")) != -1) {
    switch(c) {
      case 'x':
        xcenter = atof(optarg);
        break;
      case 'y':
        ycenter = atof(optarg);
        break;
      case 's':
        scale = atof(optarg);
        break;
      case 'W':
        image_width = atoi(optarg);
        break;
      case 'H':
        image_height = atoi(optarg);
        break;
      case 'm':
        max = atoi(optarg);
        break;
      case 'o':
        outfile = optarg;
        break;
      case 'n':
        if (!isdigit(optarg[0]) || atoi(optarg) == 0) {
          printf("Input number of threads '%s' is NaN or less than 1.\nDefaulting to 1 thread.\n", optarg);
        } else {
          num_threads = atoi(optarg);
        }
        break;
      case 'h':
        show_help();
        exit(1);
        break;
    }
  }

  // Ensure valid input for number of threads
  if (num_threads > image_height) {
    printf("Number of threads exceed max number allowed.\n Defaulting to %d threads.\n", image_height);
    num_threads = image_height;
  }

  // Display the configuration of the image.
  printf("mandel: x=%lf y=%lf scale=%lf max=%d outfile=%s\n", xcenter, ycenter, scale, max, outfile);

  // Create a bitmap of the appropriate size.
  struct bitmap *bm = bitmap_create(image_width, image_height);

  // Fill it with a dark blue, for debugging
  bitmap_reset(bm, MAKE_RGBA(0, 0, 255, 0));

  // Set thread arguments
  ThreadArgs tArgs[num_threads];
  int i;
  for (i = 0; i < num_threads; i++) {
    tArgs[i].bm = bm;
    tArgs[i].max = max;
    tArgs[i].xmin = xcenter - scale;
    tArgs[i].xmax = xcenter + scale;
    tArgs[i].ymin = ycenter-scale;
    tArgs[i].ymax = ycenter+scale;
    tArgs[i].num_threads = num_threads;
  }

  // Set threads to be joinable
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Create threads
  int rc;
  pthread_t threads[num_threads];
  for (i = 0; i < num_threads; i++) {
    // printf("DEBUG: Creating thread %d.\n", i);
    tArgs[i].id = i;
    if ((rc = pthread_create(&threads[i], NULL, compute_image, (void *) &tArgs[i])) != 0) {
      printf("ERROR: Unable to create thread %d with exit code %d.", i, rc);
      exit(EXIT_FAILURE);
    }
  }

  // Join threads
  for (i = 0; i < num_threads; i++) {
    if ((rc = pthread_join(threads[i], NULL)) != 0) {
      printf("ERROR: Unabl to join thread %d with exit code %d.", i, rc);
      exit(EXIT_FAILURE);
    }
  }
  pthread_attr_destroy(&attr);

  // Save the image in the stated file.
  if (!bitmap_save(bm, outfile)) {
  fprintf(stderr, "mandel: couldn't write to %s: %s\n", outfile, strerror(errno));
  return 1;
  }

  return 0;
}

// Compute an entire Mandelbrot image, writing each point to the given bitmap.
// Scale the image to the range (xmin-xmax, ymin-ymax), limiting iterations to "max"
void *compute_image(void *args) {
  ThreadArgs *this = (ThreadArgs *) args;
  struct bitmap *bm = this->bm;
  int width = bitmap_width(bm);
  int height = bitmap_height(bm);

  // Calculate the bounds for the thread
  int yStart, yEnd;
  yStart = height/this->num_threads * this->id;
  if (this->id == this->num_threads - 1) {  // if last thread
    yEnd = height;
  } else {
    yEnd = height/this->num_threads * (this->id+ 1);
  }

  // For every pixel in the image
  int i, j;
  for (j = yStart; j < yEnd; j++) {
    for (i = 0; i < width; i++) {
      // Determine the point in x, y space for that pixel.
      double x = this->xmin + i*(this->xmax - this->xmin)/width;
      double y = this->ymin + j*(this->ymax - this->ymin)/height;

      // Compute the iterations at that point.
      int iters = iterations_at_point(x, y, this->max);

      // Set the pixel in the bitmap.
      bitmap_set(bm, i, j, iters);
    }
  }
  return NULL;
}


// Return the number of iterations at point x, y in the Mandelbrot space, up to a maximum of max.
int iterations_at_point(double x, double y, int max) {
  double x0 = x;
  double y0 = y;
  int iter = 0;

  while ((x*x + y*y <= 4) && iter < max) {
    double xt = x*x - y*y + x0;
    double yt = 2*x*y + y0;

    x = xt;
    y = yt;
    iter++;
  }
  return iteration_to_color(iter, max);
}


// Convert a iteration number to an RGBA color.
// Here, we just scale to gray with a maximum of imax.
// Modify this function to make more interesting colors.
int iteration_to_color(int i, int max) {
  int gray = 255 * i/max;
  return MAKE_RGBA(gray, gray, gray, 0);
}
