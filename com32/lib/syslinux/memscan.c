/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 H. Peter Anvin - All Rights Reserved
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
 * memscan.c
 *
 * Query the system for free memory
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <com32.h>

#include <syslinux/memscan.h>

struct e820_entry {
    uint64_t start;
    uint64_t len;
    uint32_t type;
};

int syslinux_scan_memory(scan_memory_callback_t callback, void *data)
{
    static com32sys_t ireg;
    com32sys_t oreg;
    struct e820_entry *e820buf;
    uint64_t start, len, maxlen;
    int memfound = 0;
    int rv;
    addr_t dosmem;
    const addr_t bios_data = 0x510;	/* Amount to reserve for BIOS data */

    /* Use INT 12h to get DOS memory */
    __intcall(0x12, &__com32_zero_regs, &oreg);
    dosmem = oreg.eax.w[0] << 10;
    if (dosmem < 32 * 1024 || dosmem > 640 * 1024) {
	/* INT 12h reports nonsense... now what? */
	uint16_t ebda_seg = *(uint16_t *) 0x40e;
	if (ebda_seg >= 0x8000 && ebda_seg < 0xa000)
	    dosmem = ebda_seg << 4;
	else
	    dosmem = 640 * 1024;	/* Hope for the best... */
    }
    rv = callback(data, bios_data, dosmem - bios_data, true);
    if (rv)
	return rv;

    /* First try INT 15h AX=E820h */
    e820buf = lzalloc(sizeof *e820buf);
    if (!e820buf)
	return -1;

    ireg.eax.l = 0xe820;
    ireg.edx.l = 0x534d4150;
    ireg.ebx.l = 0;
    ireg.ecx.l = sizeof(*e820buf);
    ireg.es = SEG(e820buf);
    ireg.edi.w[0] = OFFS(e820buf);

    do {
	__intcall(0x15, &ireg, &oreg);

	if ((oreg.eflags.l & EFLAGS_CF) ||
	    (oreg.eax.l != 0x534d4150) || (oreg.ecx.l < 20))
	    break;

	start = e820buf->start;
	len = e820buf->len;

	if (start < 0x100000000ULL) {
	    /* Don't rely on E820 being valid for low memory.  Doing so
	       could mean stuff like overwriting the PXE stack even when
	       using "keeppxe", etc. */
	    if (start < 0x100000ULL) {
		if (len > 0x100000ULL - start)
		    len -= 0x100000ULL - start;
		else
		    len = 0;
		start = 0x100000ULL;
	    }

	    maxlen = 0x100000000ULL - start;
	    if (len > maxlen)
		len = maxlen;

	    if (len) {
		rv = callback(data, (addr_t) start, (addr_t) len,
			      e820buf->type == 1);
		if (rv)
		    return rv;
		memfound = 1;
	    }
	}

	ireg.ebx.l = oreg.ebx.l;
    } while (oreg.ebx.l);

    lfree(e820buf);

    if (memfound)
	return 0;

    /* Next try INT 15h AX=E801h */
    ireg.eax.w[0] = 0xe801;
    __intcall(0x15, &ireg, &oreg);

    if (!(oreg.eflags.l & EFLAGS_CF) && oreg.ecx.w[0]) {
	rv = callback(data, (addr_t) 1 << 20, oreg.ecx.w[0] << 10, true);
	if (rv)
	    return rv;

	if (oreg.edx.w[0]) {
	    rv = callback(data, (addr_t) 16 << 20, oreg.edx.w[0] << 16, true);
	    if (rv)
		return rv;
	}

	return 0;
    }

    /* Finally try INT 15h AH=88h */
    ireg.eax.w[0] = 0x8800;
    if (!(oreg.eflags.l & EFLAGS_CF) && oreg.eax.w[0]) {
	rv = callback(data, (addr_t) 1 << 20, oreg.ecx.w[0] << 10, true);
	if (rv)
	    return rv;
    }

    return 0;
}
