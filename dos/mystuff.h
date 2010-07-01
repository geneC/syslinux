#ifndef MYSTUFF_H
#define MYSTUFF_H

#include <inttypes.h>

#define NULL ((void *)0)

unsigned int skip_atou(const char **s);
unsigned int atou(const char *s);

static inline int isdigit(int ch)
{
    return (ch >= '0') && (ch <= '9');
}

struct diskio {
    uint32_t startsector;
    uint16_t sectors;
    uint16_t bufoffs, bufseg;
} __attribute__ ((packed));
int int25_read_sector(unsigned char drive, struct diskio *dio);
int int26_write_sector(unsigned char drive, struct diskio *dio);

#endif /* MYSTUFF_H */
