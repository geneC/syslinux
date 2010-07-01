#ifndef CTYPE_H
#define CTYPE_H

/*
 * Small subset of <ctype.h> for parsing uses, only handles ASCII
 * and passes the rest through.
 */

static inline int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
	c -= 0x20;

    return c;
}

static inline int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
	c += 0x20;

    return c;
}

#endif /* CTYPE_H */
