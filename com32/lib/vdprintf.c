/*
 * vdprintf.c
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/io.h>

#define BUFFER_SIZE	32768

struct file_info;
extern ssize_t __serial_write(struct file_info *, const void *, size_t);

static const uint16_t debug_base = 0x03f8; /* I/O base address */

int vdprintf(const char *format, va_list ap)
{
    int rv;
    char buffer[BUFFER_SIZE];
    char *p;

    rv = vsnprintf(buffer, BUFFER_SIZE, format, ap);

    if (rv < 0)
	return rv;

    if (rv > BUFFER_SIZE - 1)
	rv = BUFFER_SIZE - 1;

    /*
     * This unconditionally outputs to a serial port at 0x3f8 regardless of
     * if one is enabled or not (this means we don't have to enable the real
     * serial console and therefore get conflicting output.)
     */
    while (rv--) {
	while ((inb(debug_base+5) & 0x20) == 0)
	    ;
	outb(*p++, debug_base);
    }
}
