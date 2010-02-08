#include <stdbool.h>
#include <stdio.h>

#include "mystuff.h"
#include "file.h"
#include "io.h"

enum {
    THR = 0,
    RBR = 0,
    DLL = 0,
    DLM = 1,
    IER = 1,
    IIR = 2,
    FCR = 2,
    LCR = 3,
    MCR = 4,
    LSR = 5,
    MSR = 6,
    SCR = 7,
};

int serial_init(struct serial_if *sif)
{
    uint16_t port = sif->port;
    uint8_t dll, dlm, lcr;

    /* Set 115200n81 */
    outb(0x83, port + LCR);
    outb(0x01, port + DLL);
    outb(0x00, port + DLM);
    (void)inb(port + IER);	/* Synchronize */
    dll = inb(port + DLL);
    dlm = inb(port + DLM);
    lcr = inb(port + LCR);
    outb(0x03, port + LCR);
    (void)inb(port + IER);	/* Synchronize */

    if (dll != 0x01 || dlm != 0x00 || lcr != 0x83)
	return -1;		/* This doesn't look like a serial port */

    /* Disable interrupts */
    outb(port + IER, 0);

    /* Enable 16550A FIFOs if available */
    outb(port + FCR, 0x01);	/* Enable FIFO */
    (void)inb(port + IER);	/* Synchronize */
    if (inb(port + IIR) < 0xc0)
	outb(port + FCR, 0x00);	/* Disable FIFOs if non-functional */
    (void)inb(port + IER);	/* Synchronize */

    return 0;
}

void serial_write(struct serial_if *sif, const void *data, size_t n)
{
    uint16_t port = sif->port;
    const char *p = data;
    uint8_t lsr;

    while (n--) {
	do {
	    lsr = inb(port + LSR);
	} while (!(lsr & 0x20));

	outb(*p++, port + THR);
    }
}

void serial_read(struct serial_if *sif, void *data, size_t n)
{
    uint16_t port = sif->port;
    char *p = data;
    uint8_t lsr;

    while (n--) {
	do {
	    lsr = inb(port + LSR);
	} while (!(lsr & 0x01));

	*p++ = inb(port + RBR);
    }
}
