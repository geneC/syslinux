/*
 * sys/times.h
 */

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <stdint.h>
#include <core/jiffies.h>

struct tms {
    /* Empty */
};

#define CLK_TCK		HZ

typedef uint32_t clock_t;

static inline clock_t times(struct tms *__dummy)
{
    (void)__dummy;
    return jiffies();
}

#endif /* _SYS_TIMES_H */
