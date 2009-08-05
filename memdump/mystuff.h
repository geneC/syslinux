#ifndef MYSTUFF_H
#define MYSTUFF_H

#include <stdlib.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

unsigned int skip_atou(const char **s);
unsigned long strtoul(const char *, char **, int);

static inline int isdigit(int ch)
{
    return (ch >= '0') && (ch <= '9');
}

#endif /* MYSTUFF_H */
