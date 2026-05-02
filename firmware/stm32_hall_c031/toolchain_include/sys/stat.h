#ifndef STM32_HALL_SYS_STAT_H
#define STM32_HALL_SYS_STAT_H

typedef unsigned int mode_t;

struct stat {
  mode_t st_mode;
};

#define S_IFCHR 0020000

#endif
