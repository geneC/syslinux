#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
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
 * line_input.c
 *
 * Line-oriented input discupline
 */

#include "file.h"

ssize_t __line_input(struct file_info *fp, char *buf, size_t bufsize,
		     ssize_t (*get_char)(struct file_info *, void *, size_t))
{
  size_t n = 0;
  char ch;

  for(;;) {
    if ( get_char(fp, &ch, 1) != 1 )
      return n;
    
    switch ( ch ) {
    case '\r':
      *buf = '\n';
      fp->ops->write(fp, "\n", 1);
      return n+1;
    
    case '\b':
      if ( n > 0 ) {
	n--; buf--;
	fp->ops->write(fp, "\b \b", 3);
      }
      break;

    case '\x15':		/* Ctrl-U */
      while ( n ) {
	n--; buf--;
	fp->ops->write(fp, "\b \b", 3);
      }
      break;
      
    default:
      if ( n < bufsize-1 ) {
	*buf = ch;
	fp->ops->write(fp, buf, 1);
	n++;
	buf++;
      }
      break;
    }
  }
}

