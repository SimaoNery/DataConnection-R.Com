#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

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

#define TIME_DIFF(ti, tf) ((tf.tv_sec - ti.tv_sec) + (tf.tv_usec - ti.tv_usec) / 1e6)

uint8_t *ultoua(size_t n);
size_t  uatoi(uint8_t *n, uint8_t size);
int     spError(char *funcName, int isRead);
int     err(char *funcName, char *err);
void    info(char *funcName, char *msg);

#endif