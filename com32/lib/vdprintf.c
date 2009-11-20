/*
 * vdprintf.c
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/io.h>
#include <sys/cpu.h>

#undef DEBUG
#define DEBUG 1
#include <dprintf.h>

#define BUFFER_SIZE	32768

static const uint16_t debug_base = 0x03f8; /* I/O base address */

void vdprintf(const char *format, va_list ap)
{
    int rv;
    char buffer[BUFFER_SIZE];
    char *p;

    rv = vsnprintf(buffer, BUFFER_SIZE, format, ap);

    if (rv < 0)
	return;

    if (rv > BUFFER_SIZE - 1)
	rv = BUFFER_SIZE - 1;

    /*
     * This unconditionally outputs to a serial port at 0x3f8 regardless of
     * if one is enabled or not (this means we don't have to enable the real
     * serial console and therefore get conflicting output.)
     */
    p = buffer;
    while (rv--) {
	while ((inb(debug_base+5) & 0x20) == 0)
	    cpu_relax();
	outb(*p++, debug_base);
    }
}
