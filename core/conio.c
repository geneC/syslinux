/*
 * -----------------------------------------------------------------------
 *
 *   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * -----------------------------------------------------------------------
 *
 *
 * conio.c
 *
 * Console I/O code, except:
 *   writechr, writestr_early	- module-dependent
 *   writestr, crlf		- writestr.inc
 *   writehex*			- writehex.inc
 */
#include <sys/io.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <fs.h>
#include <com32.h>
#include <sys/cpu.h>

#include "bios.h"
#include "graphics.h"

union screen _cursor;
union screen _screensize;

/*
 * Serial console stuff.
 */
uint16_t SerialPort = 0;	    /* Serial port base (or 0 for no serial port) */
uint16_t BaudDivisor = 115200/9600; /* Baud rate divisor */
uint8_t FlowOutput = 0;		    /* Output to assert for serial flow */
uint8_t FlowInput = 0;		    /* Input bits for serial flow */
uint8_t FlowIgnore = 0;		    /* Ignore input unless these bits set */

uint8_t ScrollAttribute = 0x07; /* Grey on white (normal text color) */
uint16_t DisplayCon = 0x01;	/* Display console enabled */
static uint8_t TextAttribute;	/* Text attribute for message file */
static uint8_t DisplayMask;	/* Display modes mask */

/* Routine to interpret next print char */
static void (*NextCharJump)(char);

void msg_initvars(void);
static void msg_setfg(char data);
static void msg_putchar(char ch);

/*
 * loadkeys:	Load a LILO-style keymap
 *
 * Returns 0 on success, or -1 on error.
 */
int loadkeys(char *filename)
{
	FILE *f;

	f = fopen(filename, "r");
	if (!f)
		return -1;

	fread(KbdMap, 1, sizeof(KbdMap), f);

	fclose(f);
	return 0;
}

/*
 *
 * get_msg_file: Load a text file and write its contents to the screen,
 *               interpreting color codes.
 *
 * Returns 0 on success, -1 on failure.
 */
int get_msg_file(char *filename)
{
	FILE *f;
	char ch;

	f = fopen(filename, "r");
	if (!f)
		return -1;

	TextAttribute = 0x7;	/* Default grey on white */
	DisplayMask = 0x7;	/* Display text in all modes */
	msg_initvars();

	/*
	 * Read the text file a byte at a time and interpret that
	 * byte.
	 */
	while ((ch = getc(f)) != EOF) {
		/* DOS EOF? */
		if (ch == 0x1A)
			break;

		/*
		 * 01h = text mode
		 * 02h = graphics mode
		 */
		UsingVGA &= 0x1;
		UsingVGA += 1;

		NextCharJump(ch);	/* Do what shall be done */
	}

	fclose(f);
	return 0;
}

static inline void msg_beep(void)
{
	com32sys_t ireg, oreg;

	ireg.eax.w[0] = 0x0E07;	/* Beep */
	ireg.ebx.w[0] = 0x0000;
	__intcall(0x10, &ireg, &oreg);
}

/*
 * write_serial: If serial output is enabled, write character on
 * serial port.
 */
void write_serial(char data)
{
	if (!SerialPort)
		return;

	while (1) {
		char ch;

		ch = inb(SerialPort + 5); /* LSR */

		/* Wait for space in transmit register */
		if (!(ch & 0x20))
			continue;

		/* Wait for input flow control */
		ch = inb(SerialPort + 6);
		ch &= FlowInput;
		if (ch != FlowInput)
			continue;

		break;
	}

	outb(data, SerialPort);	/* Send data */
	io_delay();
}

void pm_write_serial(com32sys_t *regs)
{
	write_serial(regs->eax.b[0]);
}

void pm_serialcfg(com32sys_t *regs)
{
	uint8_t al, ah;

	regs->eax.w[0] = SerialPort;
	regs->ecx.w[0] = BaudDivisor;

	al = FlowOutput;
	ah = FlowInput;

	al |= ah;
	ah = FlowIgnore;
	ah >>= 4;

	if (!DisplayCon)
		ah |= 0x80;

	regs->ebx.w[0] = al | (ah << 8);
}

static void write_serial_displaymask(char data)
{
	if (DisplayMask & 0x4)
		write_serial(data);
}

/*
 * write_serial_str: write_serial for strings
 */
void write_serial_str(char *data)
{
	char ch;

	while ((ch = *data++))
		write_serial(ch);
}

/*
 * write_serial_str_displaymask: d:o, but ignore if DisplayMask & 04h == 0
 */
static void write_serial_str_displaymask(char *data)
{
	if (DisplayMask & 0x4)
		write_serial_str(data);
}

/*
 * pollchar: check if we have an input character pending
 *
 * Returns 1 if character pending.
 */
int pollchar(void)
{
	com32sys_t ireg, oreg;
	uint8_t data = 0;

	memset(&ireg, 0, sizeof(ireg));

	ireg.eax.b[1] = 0x11;	/* Poll keyboard */
	__intcall(0x16, &ireg, &oreg);

	if (!(oreg.eflags.l & EFLAGS_ZF))
		return 1;

	if (SerialPort) {
		cli();

		/* Already-queued input? */
		if (SerialTail == SerialHead) {
			/* LSR */
			data = inb(SerialPort + 5) & 1;
			if (data) {
				/* MSR */
				data = inb(SerialPort + 6);

				/* Required status bits */
				if (data) {
					data &= FlowIgnore;
					if (data != FlowIgnore)
						data = 0;
					else
						data = 1;
				}
			}
		}
		sti();
	}

	return data;
}

void pm_pollchar(com32sys_t *regs)
{
	if (pollchar())
		regs->eflags.l &= ~EFLAGS_ZF;
	else
		regs->eflags.l |= EFLAGS_ZF;
}

extern void do_idle(void);

/*
 * getchar: Read a character from keyboard or serial port
 */
char getchar(char *hi)
{
	com32sys_t ireg, oreg;
	unsigned char data;

	memset(&ireg, 0, sizeof(ireg));
	memset(&oreg, 0, sizeof(oreg));
	while (1) {
		call16(do_idle, &zero_regs, NULL);

		ireg.eax.b[1] = 0x11;	/* Poll keyboard */
		__intcall(0x16, &ireg, &oreg);

		if (oreg.eflags.l & EFLAGS_ZF) {
			if (!SerialPort)
				continue;

			cli();
			if (SerialTail != SerialHead) {
				/* serial queued */
				sti(); /* We already know we'll consume data */
				data = *SerialTail++;

				SerialTail = (char *)((unsigned long)SerialTail & (serial_buf_size - 1));
			} else {
				/* LSR */
				data = inb(SerialPort + 5) & 1;
				if (!data) {
					sti();
					continue;
				}
				data = inb(SerialPort + 6);
				data &= FlowIgnore;
				if (data != FlowIgnore) {
					sti();
					continue;
				}

				data = inb(SerialPort);
				sti();
				break;
			}
		} else {
			/* Keyboard input? */
			ireg.eax.b[1] = 0x10; /* Get keyboard input */
			__intcall(0x16, &ireg, &oreg);

			data = oreg.eax.b[0];
			*hi = oreg.eax.b[1];

			if (data == 0xE0)
				data = 0;

			if (data) {
				/* Convert character sets */
				data = KbdMap[data];
			}
		}

		break;
	}

	reset_idle();		/* Character received */
	return data;
}

void pm_getchar(com32sys_t *regs)
{
	regs->eax.b[0] = getchar((char *)&regs->eax.b[1]);
}

static void msg_setbg(char data)
{
	if (unhexchar(&data) == 0) {
		data <<= 4;
		if (DisplayMask & UsingVGA)
			TextAttribute = data;

		NextCharJump = msg_setfg;
	} else {
		TextAttribute = 0x7;	/* Default attribute */
		NextCharJump = msg_putchar;
	}
}

static void msg_setfg(char data)
{
	if (unhexchar(&data) == 0) {
		if (DisplayMask & UsingVGA) {
			/* setbg set foreground to 0 */
			TextAttribute |= data;
		}
	} else
		TextAttribute = 0x7;	/* Default attribute */

	NextCharJump = msg_putchar;
}

static inline void msg_ctrl_o(void)
{
	NextCharJump = msg_setbg;
}

static void msg_gotoxy(void)
{
	com32sys_t ireg, oreg;

	memset(&ireg, 0, sizeof(ireg));

	ireg.ebx.b[1] = *(uint8_t *)BIOS_page;
	ireg.edx.w[0] = CursorDX;
	ireg.eax.b[1] = 0x02;	/* Set cursor position */

	__intcall(0x10, &ireg, &oreg);
}

static void msg_newline(void)
{
	com32sys_t ireg, oreg;
	char crlf_msg[] = { '\r', '\n', '\0' };

	write_serial_str_displaymask(crlf_msg);

	if (!(DisplayMask & UsingVGA))
		return;

	CursorCol = 0;
	if ((CursorRow + 1) <= VidRows)
		CursorRow++;
	else {
		ireg.ecx.w[0] = 0x0;	/* Upper left hand corner */
		ireg.edx.w[0] = ScreenSize;

		CursorRow = ireg.edx.b[1]; /* New cursor at the bottom */

		ireg.ebx.b[1] = ScrollAttribute;
		ireg.eax.w[0] = 0x0601; /* Scroll up one line */

		__intcall(0x10, &ireg, &oreg);
	}

	msg_gotoxy();
}

static void msg_formfeed(void)
{
	char crff_msg[] = { '\r', '\f', '\0' };

	write_serial_str_displaymask(crff_msg);

	if (DisplayMask & UsingVGA) {
		com32sys_t ireg, oreg;

		memset(&ireg, 0, sizeof(ireg));

		CursorDX = 0x0;	/* Upper left hand corner */

		ireg.edx.w[0] = ScreenSize;
		ireg.ebx.b[1] = TextAttribute;

		ireg.eax.w[0] = 0x0600; /* Clear screen region */
		__intcall(0x10, &ireg, &oreg);

		msg_gotoxy();
	}
}

static void msg_novga(void)
{
	syslinux_force_text_mode();
	msg_initvars();
}

static void msg_viewimage(void)
{
	FILE *f;

	*VGAFilePtr = '\0';	/* Zero-terminate filename */

	mangle_name(VGAFileMBuf, VGAFileBuf);
	f = fopen(VGAFileMBuf, "r");
	if (!f) {
		/* Not there */
		NextCharJump = msg_putchar;
		return;
	}

	vgadisplayfile(f);
	fclose(f);
	msg_initvars();
}

/*
 * Getting VGA filename
 */
static void msg_filename(char data)
{
	/* <LF> = end of filename */
	if (data == 0x0A) {
		msg_viewimage();
		return;
	}

	/* Ignore space/control char */
	if (data > ' ') {
		if ((char *)VGAFilePtr < (VGAFileBuf + sizeof(VGAFileBuf)))
			*VGAFilePtr++ = data;
	}
}

static void msg_vga(void)
{
	NextCharJump = msg_filename;
	VGAFilePtr = (uint16_t *)VGAFileBuf;
}

static void msg_line_wrap(void)
{
	if (!(DisplayMask & UsingVGA))
		return;

	CursorCol = 0;
	if ((CursorRow + 1) <= VidRows)
		CursorRow++;
	else {
		com32sys_t ireg, oreg;

		memset(&ireg, 0, sizeof(ireg));

		ireg.ecx.w[0] = 0x0;	   /* Upper left hand corner */
		ireg.edx.w[0] = ScreenSize;

		CursorRow = ireg.edx.b[1]; /* New cursor at the bottom */

		ireg.ebx.b[1] = ScrollAttribute;
		ireg.eax.w[0] = 0x0601; /* Scroll up one line */

		__intcall(0x10, &ireg, &oreg);
	}

	msg_gotoxy();
}

static void msg_normal(char data)
{
	com32sys_t ireg, oreg;

	/* Write to serial port */
	write_serial_displaymask(data);

	if (!(DisplayMask & UsingVGA))
		return;		/* Not screen */

	if (!(DisplayCon & 0x01))
		return;

	memset(&ireg, 0, sizeof(ireg));

	ireg.ebx.b[0] = TextAttribute;
	ireg.ebx.b[1] = *(uint8_t *)BIOS_page;
	ireg.eax.b[0] = data;
	ireg.eax.b[1] = 0x09;	/* Write character/attribute */
	ireg.ecx.w[0] = 1;	/* One character only */

	/* Write to screen */
	__intcall(0x10, &ireg, &oreg);

	if ((CursorCol + 1) <= VidCols) {
		CursorCol++;
		msg_gotoxy();
	} else
		msg_line_wrap(); /* Screen wraparound */
}

static void msg_modectl(char data)
{
	data &= 0x07;
	DisplayMask = data;
	NextCharJump = msg_putchar;
}

static void msg_putchar(char ch)
{
	/* 10h to 17h are mode controls */
	if (ch >= 0x10 && ch < 0x18) {
		msg_modectl(ch);
		return;
	}

	switch (ch) {
	case 0x0F:		/* ^O = color code follows */
		msg_ctrl_o();
		break;
	case 0x0D:		/* Ignore <CR> */
		break;
	case 0x0A:		/* <LF> = newline */
		msg_newline();
		break;
	case 0x0C:		/* <FF> = clear screen */
		msg_formfeed();
		break;
	case 0x07:		/* <BEL> = beep */
		msg_beep();
		break;
	case 0x19:		/* <EM> = return to text mode */
		msg_novga();
		break;
	case 0x18:		/* <CAN> = VGA filename follows */
		msg_vga();
		break;
	default:
		msg_normal(ch);
		break;
	}
}

/*
 * Subroutine to initialize variables, also needed after loading
 * graphics file.
 */
void msg_initvars(void)
{
	com32sys_t ireg, oreg;

	ireg.eax.b[1] = 0x3;	/* Read cursor position */
	ireg.ebx.b[1] = *(uint8_t *)BIOS_page;
	__intcall(0x10, &ireg, &oreg);

	CursorDX = oreg.edx.w[0];

	/* Initialize state machine */
	NextCharJump = msg_putchar;
}
