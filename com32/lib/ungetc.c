/*
 * ungetc.c
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int ungetc(int c, FILE *f)
{
    unsigned char ch = c;

    if (unread(fileno(f), &ch, 1) == 1)
	return ch;
    else
	return EOF;
}
