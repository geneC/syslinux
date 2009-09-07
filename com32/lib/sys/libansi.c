/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
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
 * -----------------------------------------------------------------------
 *  Ansi Sequences can be found here :
 *  http://ascii-table.com/ansi-escape-sequences-vt-100.php
 *  http://en.wikipedia.org/wiki/ANSI_escape_code
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "libansi.h"

void display_cursor(bool status)
{
	if (status == true) {
		fputs(CSI "?25h", stdout);
	} else {
		fputs(CSI "?25l", stdout);
	}
}

void clear_end_of_line() {
	fputs(CSI "0K", stdout);
}

void move_cursor_left(int count) {
	char buffer[10];
	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,CSI "%dD",count);
	fputs(buffer, stdout);
}

void move_cursor_right(int count) {
	char buffer[10];
	memset(buffer,0,sizeof(buffer));
	sprintf(buffer, CSI "%dC", count);
	fputs(buffer, stdout);
}

void set_cursor_blink(bool status) {
	if (status == true)
		fputs("\033[05m",stdout);
	else
		fputs("\033[0m",stdout);
}

void clear_line() {
	fputs(CSI "2K", stdout);
}

void clear_beginning_of_line() {
	fputs(CSI "1K", stdout);
}

void move_cursor_to_column(int count) {
	char buffer[10];
        memset(buffer,0,sizeof(buffer));
	sprintf(buffer, CSI "%dG", count);
	fputs(buffer, stdout);
}

void move_cursor_to_next_line() {
	fputs("\033e", stdout);
}

void disable_utf8() {
	fputs("\033%@", stdout);
}

void set_g1_special_char(){
	fputs("\033)0", stdout);
}

void set_us_g0_charset() {
	fputs("\033(B\1#0", stdout);
}

void clear_entire_screen() {
	fputs(CSI "2J", stdout);
}

/**
 * cprint_vga2ansi - given a VGA attribute, print a character
 * @chr:	character to print
 * @attr:	vga attribute
 *
 * Convert the VGA attribute @attr to an ANSI escape sequence and
 * print it.
 **/
static void cprint_vga2ansi(const char chr, const char attr)
{
	static const char ansi_char[8] = "04261537";
	static uint16_t last_attr = 0x300;
	char buf[16], *p;

	if (attr != last_attr) {
        bool reset = false;
		p = buf;
		*p++ = '\033';
		*p++ = '[';

		if (last_attr & ~attr & 0x88) {
			*p++ = '0';
			*p++ = ';';
			/* Reset last_attr to unknown to handle
			 * background/foreground attributes correctly */
			last_attr = 0x300;
            reset = true;
		}
		if (attr & 0x08) {
			*p++ = '1';
			*p++ = ';';
		}
		if (attr & 0x80) {
			*p++ = '4';
			*p++ = ';';
		}
		if (reset || (attr ^ last_attr) & 0x07) {
			*p++ = '3';
			*p++ = ansi_char[attr & 7];
			*p++ = ';';
		}
		if (reset || (attr ^ last_attr) & 0x70) {
			*p++ = '4';
			*p++ = ansi_char[(attr >> 4) & 7];
			*p++ = ';';
		}
		p[-1] = 'm';	/* We'll have generated at least one semicolon */
		p[0] = '\0';

		last_attr = attr;

		fputs(buf, stdout);
	}

	putchar(chr);
}

void reset_colors()
{
    csprint(CSI "1D", 0x07);
}

/**
 * cprint - given a VGA attribute, print a single character at cursor
 * @chr:	character to print
 * @attr:	VGA attribute
 * @times:	number of times to print @chr
 *
 * Note: @attr is a VGA attribute.
 **/
void cprint(const char chr, const char attr, unsigned int times)
{
	while (times--)
		cprint_vga2ansi(chr, attr);
}

/**
 * csprint - given a VGA attribute, print a NULL-terminated string
 * @str:	string to print
 * @attr:	VGA attribute
 **/
void csprint(const char *str, const char attr)
{
	while (*str) {
		cprint(*str, attr, 1);
		str++;
	}
}

/**
 * clearwindow - fill a given a region on the screen
 * @top, @left, @bot, @right:	coordinates to fill
 * @fillchar:			character to use to fill the region
 * @fillattr:			character attribute (VGA)
 **/
void clearwindow(const char top, const char left, const char bot,
		 const char right, const char fillchar, const char fillattr)
{
	char x;
	for (x = top; x < bot + 1; x++) {
		gotoxy(x, left);
		cprint(fillchar, fillattr, right - left + 1);
	}
}


