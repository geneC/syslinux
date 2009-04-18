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

void display_cursor(bool status)
{
	if (status == true) {
		fputs("\033[?25h", stdout);
	} else {
		fputs("\033[?25l", stdout);
	}
}

void clear_end_of_line() {
	fputs("\033[0K", stdout);
}

void move_cursor_left(int count) {
	char buffer[10];
	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"\033[%dD",count);
	fputs(buffer, stdout);
}

void move_cursor_right(int count) {
	char buffer[10];
	memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"\033[%dC",count);
	fputs(buffer, stdout);
}

void clear_line() {
	fputs("\033[2K", stdout);
}

void clear_beginning_of_line() {
	fputs("\033[1K", stdout);
}

void move_cursor_to_column(int count) {
	char buffer[10];
        memset(buffer,0,sizeof(buffer));
	sprintf(buffer,"\033[%dG",count);
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
	fputs("\033[2J", stdout);
}
