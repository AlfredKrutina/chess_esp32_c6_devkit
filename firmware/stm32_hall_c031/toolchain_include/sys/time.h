#ifndef STM32_HALL_SYS_TIME_H
#define STM32_HALL_SYS_TIME_H

#include <stdint.h>
#include <time.h>

struct timeval {
  time_t tv_sec;
  long tv_usec;
};

#endif
