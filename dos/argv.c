/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * argv.c
 *
 * Parse a single C string into argc and argv (argc is return value.)
 * memptr points to available memory.
 */

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#define ALIGN_UP(p,t)       ((t *)(((uintptr_t)(p) + (sizeof(t)-1)) & ~(sizeof(t)-1)))

extern char __heap_start[];
void *__mem_end = &__heap_start;	/* Global variable for use by malloc() */

int __parse_argv(char ***argv, const char *str)
{
    char *mem = __mem_end;
    const char *p = str;
    char *q = mem;
    char *r;
    char **arg;
    int wasspace = 0;
    int argc = 1;

    /* First copy the string, turning whitespace runs into nulls */
    for (p = str;; p++) {
	if (*p <= ' ') {
	    if (!wasspace) {
		wasspace = 1;
		*q++ = '\0';
	    }
	} else {
	    if (wasspace) {
		argc++;
		wasspace = 0;
	    }
	    *q++ = *p;
	}

	/* This test is AFTER we have processed the null byte;
	   we treat it as a whitespace character so it terminates
	   the last argument */
	if (!*p)
	    break;
    }

    /* Now create argv */
    arg = ALIGN_UP(q, char *);
    *argv = arg;
    *arg++ = mem;		/* argv[0] */

    q--;			/* Point q to final null */
    for (r = mem; r < q; r++) {
	if (*r == '\0') {
	    *arg++ = r + 1;
	}
    }

    *arg++ = NULL;		/* Null pointer at the end */
    __mem_end = arg;		/* End of memory we used */

    return argc;
}
