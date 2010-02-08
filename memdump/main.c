/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mystuff.h"
#include "ymsend.h"
#include "srecsend.h"
#include "io.h"

const char *program = "memdump";

void __attribute__ ((noreturn)) die(const char *msg)
{
    puts(program);
    puts(": ");
    puts(msg);
    putchar('\n');
    exit(1);
}

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

static inline __attribute__ ((const))
uint16_t ds(void)
{
    uint16_t v;
asm("movw %%ds,%0":"=rm"(v));
    return v;
}

#define GDT_ENTRY(flags,base,limit)		\
	(((uint64_t)(base & 0xff000000) << 32) |	\
	 ((uint64_t)flags << 40) |			\
	 ((uint64_t)(limit & 0x00ff0000) << 32) |	\
	 ((uint64_t)(base & 0x00ffff00) << 16) |	\
	 ((uint64_t)(limit & 0x0000ffff)))

static void get_bytes(void *buf, size_t len, struct file_info *finfo,
		      size_t pos)
{
    size_t end;
    static uint64_t gdt[6];
    size_t bufl;

    pos += (size_t) finfo->pvt;	/* Add base */
    end = pos + len;

    if (end <= 0x100000) {
	/* Can stay in real mode */
	asm volatile ("movw %3,%%fs ; "
		      "fs; rep; movsl ; "
		      "movw %2,%%cx ; "
		      "rep; movsb"::"D" (buf), "c"(len >> 2),
		      "r"((uint16_t) (len & 3)), "rm"((uint16_t) (pos >> 4)),
		      "S"(pos & 15)
		      :"memory");
    } else {
	bufl = (ds() << 4) + (size_t) buf;
	gdt[2] = GDT_ENTRY(0x0093, pos, 0xffff);
	gdt[3] = GDT_ENTRY(0x0093, bufl, 0xffff);
	asm volatile ("pushal ; int $0x15 ; popal"::"a" (0x8700),
		      "c"((len + 1) >> 1), "S"(&gdt)
		      :"memory");
    }
}

int main(int argc, char *argv[])
{
    uint16_t bios_ports[4];
    const char *prefix;
    char filename[1024];
    int i;
    static struct serial_if sif = {
	.read = serial_read,
	.write = serial_write,
    };
    struct file_info finfo;
    const char ymodem_banner[] = "Now begin Ymodem download...\r\n";
    bool srec = false;

    if (argv[1][0] == '-') {
	srec = argv[1][1] == 's';
	argc--;
	argv++;
    }

    if (argc < 4)
	die("usage: memdump [-s] port prefix start,len...");

    finfo.pvt = (void *)0x400;
    get_bytes(bios_ports, 8, &finfo, 0);	/* Get BIOS serial ports */

    for (i = 0; i < 4; i++)
	printf("ttyS%i (COM%i) is at %#x\n", i, i + 1, bios_ports[i]);

    sif.port = strtoul(argv[1], NULL, 0);
    if (sif.port <= 3) {
	sif.port = bios_ports[sif.port];
    }

    if (serial_init(&sif))
	die("failed to initialize serial port");

    prefix = argv[2];

    if (!srec) {
	puts("Printing prefix...\n");
	sif.write(&sif, ymodem_banner, sizeof ymodem_banner - 1);
    }

    for (i = 3; i < argc; i++) {
	uint32_t start, len;
	char *ep;

	start = strtoul(argv[i], &ep, 0);
	if (*ep != ',')
	    die("invalid range specification");
	len = strtoul(ep + 1, NULL, 0);

	sprintf(filename, "%s%#x-%#x.bin", prefix, start, len);
	finfo.name = filename;
	finfo.size = len;
	finfo.pvt = (void *)start;

	puts("Sending ");
	puts(filename);
	puts("...\n");

	if (srec)
	    send_srec(&sif, &finfo, get_bytes);
	else
	    send_ymodem(&sif, &finfo, get_bytes);
    }

    if (!srec) {
	puts("Sending closing signature...\n");
	end_ymodem(&sif);
    }

    return 0;
}
