/*
 * SREC send routine.
 */

#include <string.h>
#include <stdio.h>
#include "srecsend.h"

static void make_srec(struct serial_if *sif, char type, size_t addr,
		      const void *data, size_t len)
{
    char buf[80];		/* More than the largest possible size */
    char *p;
    const uint8_t *dp = data;
    size_t alen = (type == '0') ? 4 : 8;
    uint8_t csum;

    p = buf;
    p += sprintf(p, "S%c%02X%0*zX", type, len+alen+1, alen, addr);
    
    csum = (len+alen+1) + addr + (addr >> 8) + (addr >> 16) + (addr >> 24);
    while (len) {
	p += sprintf(p, "%02X", *dp);
	csum += *dp;
	dp++;
    }
    csum = 0xff - csum;
    p += sprintf(p, "%02X\r\n", csum);

    sif->write(sif, buf, p-buf);
}

void send_srec(struct serial_if *sif, struct file_info *fileinfo,
	       void (*gen_data) (void *, size_t, struct file_info *, size_t))
{
    uint8_t blk_buf[1024];
    const uint8_t *np;
    size_t addr, len, bytes, chunk, offset, pos;
    int blk;

    len = fileinfo->size;

    make_srec(sif, '0', 0, NULL, 0);

    blk = 0;
    pos = 0;
    addr = fileinfo->base;
    while (len) {
	gen_data(blk_buf, sizeof blk_buf, fileinfo, pos);
	pos += sizeof blk_buf;
	bytes = sizeof blk_buf;
	if (bytes > len)
	    bytes = len;
	len -= bytes;

	printf("Sending block %d...\r", blk);
	
	np = blk_buf;
	while (bytes) {
	    chunk = bytes > 32 ? 32 : bytes;

	    make_srec(sif, '3', addr, np, chunk);

	    bytes -= chunk;
	    offset += chunk;
	    np += chunk;
	    addr += chunk;
	}
	blk++;
    }

    printf("\nSending EOT...\n");
    make_srec(sif, '7', fileinfo->base, NULL, 0);
    printf("Done.\n");
}
