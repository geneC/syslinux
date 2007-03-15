/*
 * sys/times.h
 */

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <stdint.h>

struct tms {
  /* Empty */
};

#define HZ		18	/* Piddly resolution... */
#define CLK_TCK		HZ

typedef uint16_t clock_t;

clock_t times(struct tms *);

#endif /* _SYS_TIMES_H */
