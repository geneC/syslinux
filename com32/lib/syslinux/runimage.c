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
 * runimage.c
 *
 * Load and run a syslinux image.
 */

#include <stdlib.h>
#include <string.h>
#include <syslinux/boot.h>
#include <com32.h>

void syslinux_run_kernel_image(const char *filename, const char *cmdline,
			       uint32_t ipappend_flags, uint32_t type)
{
    static com32sys_t ireg;
    char *bbfilename = NULL;
    char *bbcmdline  = NULL;

    bbfilename = lstrdup(filename);
    if (!bbfilename)
	goto fail;

    bbcmdline = lstrdup(cmdline);
    if (!bbcmdline)
	goto fail;


    ireg.eax.w[0] = 0x0016;
    ireg.ds = SEG(bbfilename);
    /* ireg.esi.w[0] = OFFS(bbfilename); */
    ireg.es = SEG(bbcmdline);
    /* ireg.ebx.w[0] = OFFS(bbcmdline); */
    ireg.ecx.l = ipappend_flags;
    ireg.edx.l = type;

    __intcall(0x22, &ireg, 0);

fail:
    if (bbcmdline)
	lfree(bbcmdline);
    if (bbfilename)
	lfree(bbfilename);
}
