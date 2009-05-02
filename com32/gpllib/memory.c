/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   Some parts borrowed from meminfo.c32:
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Some parts borrowed from Linux:
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author H. Peter Anvin
 *
 *   Interrupt list from Ralf Brown (http://www.cs.cmu.edu/~ralf/files.html)
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <stdint.h>
#include <com32.h>
#include <string.h>
#include <memory.h>

const char * const e820_types[] = {
	"usable",
	"reserved",
	"ACPI reclaim",
	"ACPI NVS",
	"unusable",
};

struct e820_ext_entry {
	struct e820entry std;
	uint32_t ext_flags;
} __attribute__((packed));

#define SMAP	0x534d4150	/* ASCII "SMAP" */

void get_type(int type, char *type_ptr, int type_ptr_sz)
{
	unsigned int real_type = type - 1;
	if (real_type < sizeof(e820_types)/sizeof(e820_types[0]))
		strncpy(type_ptr, e820_types[real_type], type_ptr_sz);
}

/**
 *INT 15 - newer BIOSes - GET SYSTEM MEMORY MAP
 *	AX = E820h
 *	EAX = 0000E820h
 *	EDX = 534D4150h ('SMAP')
 *	EBX = continuation value or 00000000h to start at beginning of map
 *	ECX = size of buffer for result, in bytes (should be >= 20 bytes)
 *	ES:DI -> buffer for result (see #00581)
 *
 * Return: CF clear if successful
 *	    EAX = 534D4150h ('SMAP')
 *	    ES:DI buffer filled
 *	    EBX = next offset from which to copy or 00000000h if all done
 *	    ECX = actual length returned in bytes
 *	CF set on error
 *	    AH = error code (86h) (see #00496 at INT 15/AH=80h)
 *
 * Notes: originally introduced with the Phoenix BIOS v4.0, this function is
 *	  now supported by most newer BIOSes, since various versions of Windows
 *	  call it to find out about the system memory
 *	a maximum of 20 bytes will be transferred at one time, even if ECX is
 *	  higher; some BIOSes (e.g. Award Modular BIOS v4.50PG) ignore the
 *	  value of ECX on entry, and always copy 20 bytes
 *	some BIOSes expect the high word of EAX to be clear on entry, i.e.
 *	  EAX=0000E820h
 *	if this function is not supported, an application should fall back
 *	  to AX=E802h, AX=E801h, and then AH=88h
 *	the BIOS is permitted to return a nonzero continuation value in EBX
 *	  and indicate that the end of the list has already been reached by
 *	  returning with CF set on the next iteration
 *	this function will return base memory and ISA/PCI memory contiguous
 *	  with base memory as normal memory ranges; it will indicate
 *	  chipset-defined address holes which are not in use and motherboard
 *	  memory-mapped devices, and all occurrences of the system BIOS as
 *	  reserved; standard PC address ranges will not be reported
 *
 *	 ACPI 3.0 added the extended flags support.  If bit 0
 *	   in the extended flags is zero, we're supposed to simply
 *	   ignore the entry -- a backwards incompatible change!
 **/
void detect_memory_e820(struct e820entry *desc, int size_map, int *size_found)
{
	int count = 0;
	static struct e820_ext_entry buf; /* static so it is zeroed */

	com32sys_t ireg, oreg;
	memset(&ireg, 0, sizeof ireg);

	ireg.eax.w[0] = 0xe820;
	ireg.edx.l    = SMAP;
	ireg.ecx.l    = sizeof(struct e820_ext_entry);
	ireg.edi.w[0] = OFFS(__com32.cs_bounce);
	ireg.es       = SEG(__com32.cs_bounce);

	/*
	 * Set this here so that if the BIOS doesn't change this field
	 * but still doesn't change %ecx, we're still okay...
	 */
	memset(&buf, 0, sizeof buf);
	buf.ext_flags = 1;

	do {
		memcpy(__com32.cs_bounce, &buf, sizeof buf);

		/* Important: %edx and %esi are clobbered by some BIOSes,
		   so they must be either used for the error output
		   or explicitly marked clobbered.  Given that, assume there
		   is something out there clobbering %ebp and %edi, too. */
		__intcall(0x15, &ireg, &oreg);

		/* Some BIOSes stop returning SMAP in the middle of
		   the search loop.  We don't know exactly how the BIOS
		   screwed up the map at that point, we might have a
		   partial map, the full map, or complete garbage, so
		   just return failure. */
		if (oreg.eax.l != SMAP) {
			count = 0;
			break;
		}

		if (oreg.eflags.l & EFLAGS_CF ||
		    oreg.ecx.l < 20)
			break;

		memcpy(&buf, __com32.cs_bounce, sizeof buf);

		if (oreg.ecx.l < 24)
			buf.ext_flags = 1;

		memcpy(&desc[count], &buf, sizeof buf);
		count++;

		/* Set continuation value */
		ireg.ebx.l = oreg.ebx.l;
	} while (ireg.ebx.l && count < size_map);

	*size_found = count;
}
