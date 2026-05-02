#ifndef STM32_HALL_SYS_TIMES_H
#define STM32_HALL_SYS_TIMES_H

#include <stdint.h>

struct tms {
  clock_t tms_utime;
  clock_t tms_stime;
  clock_t tms_cutime;
  clock_t tms_cstime;
};

#endif
