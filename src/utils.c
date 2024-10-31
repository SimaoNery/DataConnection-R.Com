#include "utils.h"

static size_t ndivs(size_t n) {
  size_t res = 0;

  if (n == 0)
    return 1;
  
  if (n < 0) {
    res++;
    n *= -1;
  }
  
  while (n != 0) {
    n /= 10;
    res++;
  }
  
  return res;
}

uint8_t *ultoua(size_t n) {
  size_t i = ndivs(n);

  uint8_t *res = calloc(i + 1, sizeof(uint8_t));
  if (res == NULL)
    return NULL;
  
  res[i--] = 0;
  
  if (n == 0)
    res[i] = '0';

  if (n < 0)
    res[0] = '-';

  while (n != 0) {
    res[i--] = '0' + abs(n % 10);
    n /= 10;
  }

  return res;
}

size_t uatoi(uint8_t* n, uint8_t size)
{
    size_t ret = 0;
    if (n == NULL)
        return 0;
    for (uint8_t i = 0; i < size; i++)
        ret = ret * 10 + n[i] - '0';
    return ret;
}

int spError(char *funcName, int isRead)
{
    if (isRead)
        printf("[%s] Got error reading from serial port\n", funcName);
    else
        printf("[%s] Got error writing to serial port\n", funcName);
    return -1;
}

int err(char *funcName, char *msg)
{
    info(funcName, msg);
    return -1;
}

void info(char *funcName, char *msg)
{
    printf("[%s] %s\n", funcName, msg);
}

