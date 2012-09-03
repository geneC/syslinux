/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
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
 * keymap.h
 *
 * Map scan codes to key codes that key processing in com32/libutil expects to rely on.
 * Scan codes that are part of EFI spec but not included in the map are:
 * 	F13..F24
 * 	VOLUME UP/DOWN
 * 	BRIGHTNESS UP/DOWN
 * 	SUSPEND/HIBERNATE
 * 	TOGGLE DISPLAY
 * 	RECOVERY
 * 	EJECT
 */

#ifndef SCANKEY_MAP
#define SCANKEY_MAP

#include <getkey.h>

struct keycode {
    int code;
    int seqlen;
    const unsigned char *seq;
};

#define CODE(x,y) { x, (sizeof y)-1, (const unsigned char *)(y) }

const struct keycode keycodes[] = {
    /* First, the BIOS combined codes */
    CODE(KEY_UP, "\0\x48"),
    CODE(KEY_DOWN, "\0\x50"),
    CODE(KEY_RIGHT, "\0\x4D"),
    CODE(KEY_LEFT, "\0\x4B"),
    CODE(KEY_HOME, "\0\x47"),
    CODE(KEY_END, "\0\x4F"),
    CODE(KEY_INSERT, "\0\x52"),
    CODE(KEY_DELETE, "\0\x53"),
    CODE(KEY_PGUP, "\0\x49"),
    CODE(KEY_PGDN, "\0\x51"),
    CODE(KEY_F1, "\0\x3B"),
    CODE(KEY_F2, "\0\x3C"),
    CODE(KEY_F3, "\0\x3D"),
    CODE(KEY_F4, "\0\x3E"),
    CODE(KEY_F5, "\0\x3F"),
    CODE(KEY_F6, "\0\x40"),
    CODE(KEY_F7, "\0\x41"),
    CODE(KEY_F8, "\0\x42"),
    CODE(KEY_F9, "\0\x43"),
    CODE(KEY_F10, "\0\x44"),
    CODE(KEY_F11, "\0\x85"),
    CODE(KEY_F12, "\0\x86"),
};

#define NCODES ((int)(sizeof keycodes/sizeof(struct keycode)))
#endif /* SCANKEY_MAP */
