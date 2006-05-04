/* -*- c -*- ------------------------------------------------------------- *
 *
 *   Copyright 2004-2005 Murali Krishnan Ganapathy - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef __SCANCODES_H__
#define __SCANCODES_H__

// Scancodes of some keys
#define ESCAPE     1
#define ENTERA    28
#define ENTERB   224

#define HOMEKEY  71
#define UPARROW  72
#define PAGEUP   73
#define LTARROW  75
#define RTARROW  77
#define ENDKEY   79
#define DNARROW  80
#define PAGEDN   81
#define INSERT   82
#define DELETE   83
#define SPACEKEY 57 // Scan code for SPACE

#define CTRLLT 0x73
#define CTRLRT 0x74

#define F1  0x3B
#define F2  0x3C
#define F3  0x3D
#define F4  0x3E
#define F5  0x3F
#define F6  0x40
#define F7  0x41
#define F8  0x42
#define F9  0x43
#define F10 0x44
#define F11 0x85
#define F12 0x86

#define CTRLF1  0x5E
#define CTRLF2  0x5F
#define CTRLF3  0x60
#define CTRLF4  0x61
#define CTRLF5  0x62
#define CTRLF6  0x63
#define CTRLF7  0x64
#define CTRLF8  0x65
#define CTRLF9  0x66
#define CTRLF10 0x67
#define CTRLF11 0x89
#define CTRLF12 0x8A

#define ALTF1   0x68
#define ALTF2   0x69
#define ALTF3   0x6A
#define ALTF4   0x6B
#define ALTF5   0x6C
#define ALTF6   0x6D
#define ALTF7   0x6E
#define ALTF8   0x6F
#define ALTF9   0x70
#define ALTF10  0x71
#define ALTF11  0x8B
#define ALTF12  0x8C

/* Bits representing ShiftFlags, See Int16/Function 2 or Mem[0x417] to get this info */

#define INSERT_ON     (1<<7)
#define CAPSLOCK_ON   (1<<6)
#define NUMLOCK_ON    (1<<5)
#define SCRLLOCK_ON   (1<<4)
#define ALT_PRESSED   (1<<3)
#define CTRL_PRESSED  (1<<2)
// actually 1<<1 is Left Shift, 1<<0 is right shift
#define SHIFT_PRESSED (1<<1 | 1 <<0)

#endif
