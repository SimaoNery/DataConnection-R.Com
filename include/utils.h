#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define TROP = 0

typedef struct s_statistics {
  size_t bytes_read;
  size_t n_frames;
  size_t n_errors;
  size_t total_size;
  double time_send_control;
  double time_send_data;
  struct timeval start;
} t_statistics;

#define TIME_DIFF(x, y) ((x.tv_sec - y.tv_sec) + (x.tv_usec - y.tv_usec) / 1e6)

uint8_t *ultoua(size_t n);
size_t uatoi(uint8_t *n, uint8_t size);

#endif