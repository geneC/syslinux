/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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
 * ansicon_write.c
 *
 * Write to the screen using ANSI control codes (about as capable as
 * DOS' ANSI.SYS.)
 */

#include <errno.h>
#include <string.h>
#include <com32.h>
#include <minmax.h>
#include <colortbl.h>
#include <klibc/compiler.h>
#include <syslinux/config.h>
#include "file.h"
#include "ansi.h"

static void ansicon_erase(const struct term_state *, int, int, int, int);
static void ansicon_write_char(int, int, uint8_t, const struct term_state *);
static void ansicon_showcursor(const struct term_state *);
static void ansicon_scroll_up(const struct term_state *);
static void ansicon_set_cursor(int, int, bool);

static struct term_state ts;
struct ansi_ops __ansicon_ops = {
    .erase = ansicon_erase,
    .write_char = ansicon_write_char,
    .showcursor = ansicon_showcursor,
    .set_cursor = ansicon_set_cursor,
    .scroll_up = ansicon_scroll_up,
    .beep = __ansicon_beep,
};

static struct term_info ti = {
    .disabled = 0,
    .ts = &ts,
    .op = &__ansicon_ops
};

#define BIOS_CURXY ((struct curxy *)0x450)	/* Array for each page */
#define BIOS_ROWS (*(uint8_t *)0x484)	/* Minus one; if zero use 24 (= 25 lines) */
#define BIOS_COLS (*(uint16_t *)0x44A)
#define BIOS_PAGE (*(uint8_t *)0x462)

/* Reference counter to the screen, to keep track of if we need
   reinitialization. */
static int ansicon_counter = 0;

static uint16_t cursor_type;	/* Saved cursor pattern */

/* Common setup */
int __ansicon_open(struct file_info *fp)
{
    static com32sys_t ireg;	/* Auto-initalized to all zero */
    com32sys_t oreg;

    if (!ansicon_counter) {
	/* Are we disabled? */
	if (syslinux_serial_console_info()->flowctl & 0x8000) {
	    ti.disabled = 1;
	    ti.rows = 25;
	    ti.cols = 80;
	} else {
	    /* Force text mode */
	    ireg.eax.w[0] = 0x0005;
	    __intcall(0x22, &ireg, NULL);

	    /* Initial state */
	    ti.rows = BIOS_ROWS ? BIOS_ROWS + 1 : 25;
	    ti.cols = BIOS_COLS;
	    __ansi_init(&ti);

	    /* Get cursor shape and position */
	    ireg.eax.b[1] = 0x03;
	    ireg.ebx.b[1] = BIOS_PAGE;
	    __intcall(0x10, &ireg, &oreg);
	    cursor_type = oreg.ecx.w[0];
	    ti.ts->xy.x = oreg.edx.b[0];
	    ti.ts->xy.y = oreg.edx.b[1];
	}
    }

    fp->o.rows = ti.rows;
    fp->o.cols = ti.cols;

    ansicon_counter++;
    return 0;
}

int __ansicon_close(struct file_info *fp)
{
    (void)fp;

    ansicon_counter--;
    return 0;
}

/* Turn ANSI attributes into VGA attributes */
static uint8_t ansicon_attribute(const struct term_state *st)
{
    int bg = st->bg;
    int fg;

    if (st->underline)
	fg = 0x01;
    else if (st->intensity == 0)
	fg = 0x08;
    else
	fg = st->fg;

    if (st->reverse) {
	bg = fg & 0x07;
	fg &= 0x08;
	fg |= st->bg;
    }

    if (st->blink)
	bg ^= 0x08;

    if (st->intensity == 2)
	fg ^= 0x08;

    return (bg << 4) | fg;
}

/* Erase a region of the screen */
static void ansicon_erase(const struct term_state *st,
			  int x0, int y0, int x1, int y1)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0600;	/* Clear window */
    ireg.ebx.b[1] = ansicon_attribute(st);
    ireg.ecx.b[0] = x0;
    ireg.ecx.b[1] = y0;
    ireg.edx.b[0] = x1;
    ireg.edx.b[1] = y1;
    __intcall(0x10, &ireg, NULL);
}

/* Show or hide the cursor */
static void ansicon_showcursor(const struct term_state *st)
{
    static com32sys_t ireg;

    ireg.eax.b[1] = 0x01;
    ireg.ecx.w[0] = st->cursor ? cursor_type : 0x2020;
    __intcall(0x10, &ireg, NULL);
}

static void ansicon_set_cursor(int x, int y, bool visible)
{
    const int page = BIOS_PAGE;
    struct curxy xy = BIOS_CURXY[page];
    static com32sys_t ireg;

    (void)visible;

    if (xy.x != x || xy.y != y) {
	ireg.eax.b[1] = 0x02;
	ireg.ebx.b[1] = page;
	ireg.edx.b[1] = y;
	ireg.edx.b[0] = x;
	__intcall(0x10, &ireg, NULL);
    }
}

static void ansicon_write_char(int x, int y, uint8_t ch,
			       const struct term_state *st)
{
    static com32sys_t ireg;

    ansicon_set_cursor(x, y, false);

    ireg.eax.b[1] = 0x09;
    ireg.eax.b[0] = ch;
    ireg.ebx.b[1] = BIOS_PAGE;
    ireg.ebx.b[0] = ansicon_attribute(st);
    ireg.ecx.w[0] = 1;
    __intcall(0x10, &ireg, NULL);
}

static void ansicon_scroll_up(const struct term_state *st)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0601;
    ireg.ebx.b[1] = ansicon_attribute(st);
    ireg.ecx.w[0] = 0;
    ireg.edx.b[1] = ti.rows - 1;
    ireg.edx.b[0] = ti.cols - 1;
    __intcall(0x10, &ireg, NULL);	/* Scroll */
}

ssize_t __ansicon_write(struct file_info *fp, const void *buf, size_t count)
{
    const unsigned char *bufp = buf;
    size_t n = 0;

    (void)fp;

    if (ti.disabled)
	return count;		/* Nothing to do */

    while (count--) {
	__ansi_putchar(&ti, *bufp++);
	n++;
    }

    return n;
}

void __ansicon_beep(void)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0e07;
    ireg.ebx.b[1] = BIOS_PAGE;
    __intcall(0x10, &ireg, NULL);
}

const struct output_dev dev_ansicon_w = {
    .dev_magic = __DEV_MAGIC,
    .flags = __DEV_TTY | __DEV_OUTPUT,
    .fileflags = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND,
    .write = __ansicon_write,
    .close = __ansicon_close,
    .open = __ansicon_open,
};
