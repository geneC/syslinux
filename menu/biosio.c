/* -*- c -*- ------------------------------------------------------------- *
 *   
 *   Copyright 2004 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Bostom MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include "string.h"
#include "biosio.h"

/* BIOS Assisted output routines */

/* Print character and attribute at cursor */
static inline void asm_cprint(char chr, char attr, int times, char disppage)
{
    asm volatile("movb $0x09,%%ah ; int $0x10"
		 : "+a" (chr) : "b" (attr + (disppage << 8)), "c" (times));
}

void cprint(char chr,char attr,int times,char disppage)
{
    asm_cprint(chr,attr,times,disppage);
}

static inline void asm_setdisppage(char num)
{
    asm volatile("movb $0x05,%%ah ; int $0x10"
		 : "+a" (num));
}

void setdisppage(char num) // Set the display page to specified number
{
    asm_setdisppage(num);
}


static inline char asm_getdisppage(void)
{
    char page;
    
    asm("movb $0x0f,%%ah ; "
	"int $0x10 ; "
	"movb %%bh,%0" : "=rm" (page) : : "eax", "ebp");
    return page;
}

char getdisppage() // Get current display page 
{
    return asm_getdisppage();
}

static inline void asm_putchar(char x, char page)
{
    asm volatile("movb %1,%%bh ; movb $0x0e,%%ah ; int $0x10"
		 : "+a" (x)
		 : "g" (page)
		 : "ebx", "ebp");
}

/* Print a C string (NUL-terminated) */
void csprint(const char *str)
{
    char page = asm_getdisppage();
    
    while ( *str ) {
	asm_putchar(*str, page);
	str++;
    }
}

void clearwindow(char top, char left, char bot, char right, char page, char fillchar, char fillattr)
{
    char x;
    for (x=top; x < bot+1; x++)
    {
        gotoxy(x,left,page);
        asm_cprint(fillchar,fillattr,right-left+1,page);
    }
}

void cls(void)
{
    gotoxy(0,0,getdisppage());
    asm_cprint(' ',0x07,getnumrows()*getnumcols(),getdisppage());    
}

static inline void asm_gotoxy(char row,char col, char page)
{
    asm volatile("movb %1,%%bh ; "
		 "movb $0x02,%%ah ; "
		 "int $0x10"
		 : : "d" ((row << 8) + col), "g" (page)
		 : "eax", "ebx");
}

void gotoxy(char row,char col, char page)
{
    asm_gotoxy(row,col,page);
}

static inline void asm_getpos(char *row, char *col, char page)
{
    asm("movb %2,%%bh ; "
	"movb $0x03,%%ah ; "
	"int $0x10 ; "
	"movb %%dh,%0 ; "
	"movb %%dl,%1"
	: "=m" (*row), "=m" (*col)
	: "g" (page)
	: "eax", "ebx", "ecx", "edx");
}

void getpos(char * row, char * col, char page)
{
    asm_getpos(row,col,page);
}

char asm_inputc(char *scancode)
{
  unsigned short ax;

  asm volatile("movb $0x10,%%ah ; "
	       "int $0x16"
	       : "=a" (ax));
  
  if (scancode)
      *scancode = (ax >> 8);
  
  return (char)ax;
}
   
char inputc(char * scancode)
{
    return asm_inputc(scancode);
}

static inline void asm_cursorshape(char start, char end)
{
    asm volatile("movb $0x01,%%ah ; int $0x10"
		 : : "c" ((start << 8) + end) : "eax");
}
   
void cursoroff(void)
{
    asm_cursorshape(32,32);
}

void cursoron(void)
{
    asm_cursorshape(6,7);
}

char bkspstr[] = " \b$";
char eolstr[] = "\n$";

static inline char asm_getchar(void)
{
    char v;
    
    /* Get key without echo */
    asm("movb $0x08,%%ah ; int $0x21" : "=a" (v));
    
    return v;
}

// Reads a line of input from stdin. Replace CR with NUL byte
void getstring(char *str, unsigned int size)
{
    char c;
    char *p = str;
    char page = asm_getdisppage();
    
    while ( (c = asm_getchar()) != '\r' ) {
	switch (c) {
	case '\0':		/* Extended char prefix */
	    asm_getchar();	/* Drop */
	    break;
	case '\b':
	    if ( p > str ) {
		p--;
		csprint("\b \b");
	    }
	    break;
	case '\x15':		/* Ctrl-U: kill input */
	    while ( p > str ) {
		p--;
		csprint("\b \b");
	    }
	    break;
	default:
	    if ( c >= ' ' && (unsigned int)(p-str) < size-1 ) {
		*p++ = c;
		asm_putchar(c, page);
	    }
	    break;
	}
    }
    *p = '\0';
    csprint("\r\n");
}

static inline void asm_setvideomode(char mode)
{
    /* This BIOS function is notoriously register-dirty,
       so push/pop around it */
    asm volatile("pushal ; xorb %%ah,%%ah ; int $0x10 ; popal"
		 : : "a" (mode) );
}

void setvideomode(char mode)
{
    asm_setvideomode(mode);
}
