/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
 * pxe_get_cached.c
 *
 * PXE call "get cached info"
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <com32.h>

#include <syslinux/pxe.h>

/* Returns the status code from PXE (0 on success),
   or -1 on invocation failure */
int pxe_get_nic_type(t_PXENV_UNDI_GET_NIC_TYPE *gnt)
{
    com32sys_t regs;
    t_PXENV_UNDI_GET_NIC_TYPE *lgnt;

    lgnt = lzalloc(sizeof *lgnt);
    if (!lgnt)
	return -1;

    memset(&regs, 0, sizeof regs);
    regs.eax.w[0] = 0x0009;
    regs.ebx.w[0] = PXENV_UNDI_GET_NIC_TYPE;
    regs.es = SEG(lgnt);
    /* regs.edi.w[0] = OFFS(lgnt); */

    __intcall(0x22, &regs, &regs);

    memcpy(gnt, lgnt, sizeof(t_PXENV_UNDI_GET_NIC_TYPE));
    lfree(lgnt);

    if (regs.eflags.l & EFLAGS_CF)
	return -1;

    return gnt->Status;
}
