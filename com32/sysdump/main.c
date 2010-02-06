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
#include <inttypes.h>
#include <dprintf.h>
#include "ymsend.h"
#include "srecsend.h"

const char *program = "memdump";

void __attribute__ ((noreturn)) die(const char *msg)
{
    printf("%s: %s\n", program, msg);
    exit(1);
}

static void get_bytes(void *buf, size_t len, struct file_info *finfo,
		      size_t pos)
{
    pos += (size_t) finfo->pvt;	/* Add base */
    memcpy(buf, (void *)pos, len);
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
	puts("Printing prefix...");
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

	printf("Sending %s...\n", filename);

	if (srec)
	    send_srec(&sif, &finfo, get_bytes);
	else
	    send_ymodem(&sif, &finfo, get_bytes);
    }

    if (!srec) {
	puts("Sending closing signature...");
	end_ymodem(&sif);
    }

    return 0;
}
