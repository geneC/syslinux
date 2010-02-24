/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corporation; author: H. Peter Anvin
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
 * pxe_dns.c
 *
 * Resolve a hostname via DNS
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <com32.h>

#include <syslinux/pxe.h>

/* Returns the status code from PXE (0 on success),
   or -1 on invocation failure */
uint32_t pxe_dns(const char *hostname)
{
    com32sys_t regs;
    union {
	unsigned char b[4];
	uint32_t ip;
    } q;
    char *lm_hostname;

    /* Is this a dot-quad? */
    if (sscanf(hostname, "%hhu.%hhu.%hhu.%hhu",
	       &q.b[0], &q.b[1], &q.b[2], &q.b[3]) == 4)
	return q.ip;

    lm_hostname = lstrdup(hostname);
    if (!lm_hostname)
	return 0;

    memset(&regs, 0, sizeof regs);
    regs.eax.w[0] = 0x0010;
    regs.es = SEG(lm_hostname);
    /* regs.ebx.w[0] = OFFS(lm_hostname); */

    __intcall(0x22, &regs, &regs);

    lfree(lm_hostname);

    if (regs.eflags.l & EFLAGS_CF)
	return 0;

    return regs.eax.l;
}
