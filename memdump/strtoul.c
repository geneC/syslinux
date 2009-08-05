/*
 * strtoul.c
 *
 */

#include "mystuff.h"

static inline int isspace(int c)
{
    return (c <= ' ');		/* Close enough */
}

static inline int digitval(int ch)
{
    if (ch >= '0' && ch <= '9') {
	return ch - '0';
    } else if (ch >= 'A' && ch <= 'Z') {
	return ch - 'A' + 10;
    } else if (ch >= 'a' && ch <= 'z') {
	return ch - 'a' + 10;
    } else {
	return -1;
    }
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    int minus = 0;
    unsigned long v = 0;
    int d;

    while (isspace((unsigned char)*nptr)) {
	nptr++;
    }

    /* Single optional + or - */
    {
	char c = *nptr;
	if (c == '-' || c == '+') {
	    minus = (c == '-');
	    nptr++;
	}
    }

    if (base == 0) {
	if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
	    nptr += 2;
	    base = 16;
	} else if (nptr[0] == '0') {
	    nptr++;
	    base = 8;
	} else {
	    base = 10;
	}
    } else if (base == 16) {
	if (nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
	    nptr += 2;
	}
    }

    while ((d = digitval(*nptr)) >= 0 && d < base) {
	v = v * base + d;
	nptr++;
    }

    if (endptr)
	*endptr = (char *)nptr;

    return minus ? -v : v;
}
