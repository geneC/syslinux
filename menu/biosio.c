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

static inline void asm_putchar(char x)
{
  asm volatile("int $0x21" : : "a" (x + 0x0200));
}

static inline void asm_sprint(const char *str)
{
  asm volatile("movb $0x09,%%ah ; int $0x21" : : "d" (str) : "eax");
}
 
/* BIOS Assisted output routines */
void csprint(char *str) // Print a C str (NULL terminated)
{
    int o,l;

    o = (long)(str) & 0xffff; // Offset of location
    l = strlen(str);
    str[l] = '$'; // Replace \0 with $
    asm_sprint(str);
    str[l] = '\0'; // Change it back.
}

void sprint(const char *str)
{
    asm_sprint(str);
}


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
  register char page asm("%bh");

  asm("movb $0x0f,%%ah ; int $0x10" : "=r" (page) : : "eax", "ebx");
  return page;
}

char getdisppage() // Get current display page 
{
    return asm_getdisppage();
}

void clearwindow(char top,char left,char bot,char right, char page,char fillchar, char fillattr)
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
    asm_cprint(' ',0x07,25*80,getdisppage());    
}

static inline void asm_gotoxy(char row,char col, char page)
{
  asm volatile("movb $0x02,%%ah ; "
	       "int $0x10"
	       : : "d" ((row << 8) + col), "b" (page << 8)
	       : "eax");
}
   
void gotoxy(char row,char col, char page)
{
    asm_gotoxy(row,col,page);
}

static inline void asm_getpos(char *row, char *col, char page)
{
  asm("movb $0x03,%%ah ; "
      "int $0x10 ; "
      "movb %%dh,%0 ; "
      "movb %%dl,%1"
      : "=m" (*row), "=m" (*col)
      : "b" (page << 8)
      : "eax", "ecx", "edx");
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

void asm_cursorshape(char start, char end)
{
  asm volatile("movb $0x01,%%ah ; int $0x10"
	       : : "c" ((start << 8) + end) : "eax");
}
   
void cursoroff(void)
{
    asm_cursorshape(31,31);
}

void cursoron(void)
{
    asm_cursorshape(6,7);
}

char bkspstr[] = " \b$";
char eolstr[] = "\n$";

static char asm_getchar(void)
{
  char v;

  /* Get key without echo */
  asm("movb $0x08,%%ah ; int $0x21" : "=a" (v));

  return v;
}

void getstring(char *str, unsigned int size)
// Reads a line of input from stdin. Replace CR with NUL byte
{
  char c;
  char *p = str;

  while ( (c = asm_getchar()) != '\r' ) {
    switch (c) {
    case '\0':			/* Extended char prefix */
      asm_getchar();		/* Drop */
      break;
    case '\b':
      if ( p > str ) {
	p--;
	sprint("\b \b$");
      }
      break;
    case '\x15':		/* Ctrl-U: kill input */
      while ( p > str ) {
	p--;
	sprint("\b \b$");
      }
      break;
    default:
      if ( c >= ' ' && (unsigned int)(p-str) < size-1 ) {
	*p++ = c;
	asm_putchar(c);
      }
      break;
    }
  }
  *p = '\0';
}
