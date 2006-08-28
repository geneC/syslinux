/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 H. Peter Anvin - All Rights Reserved
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

#ifndef LIB_SYS_VESA_VIDEO_H
#define LIB_SYS_VESA_VIDEO_H

#define FONT_MAX_CHARS	256
#define FONT_MAX_HEIGHT	 32
#define FONT_WIDTH	  8

#define VIDEO_X_SIZE	640
#define VIDEO_Y_SIZE	480

#define VIDEO_BORDER	8
#define TEXT_PIXEL_ROWS (VIDEO_Y_SIZE-2*VIDEO_BORDER)
#define TEXT_PIXEL_COLS (VIDEO_X_SIZE-2*VIDEO_BORDER)

#define SHADOW_NONE	0
#define SHADOW_ALL	1
#define SHADOW_NORMAL	2
#define SHADOW_REVERSE	3

struct vesa_char {
  uint8_t ch;			/* Character */
  uint8_t attr;			/* PC-style graphics attribute */
  uint8_t sha;			/* Shadow attributes */
  uint8_t pad;			/* Currently unused */
};

extern struct vesa_char *__vesacon_text_display;

extern int __vesacon_font_height;
extern uint8_t __vesacon_graphics_font[FONT_MAX_CHARS][FONT_MAX_HEIGHT];
extern uint32_t __vesacon_background[VIDEO_Y_SIZE][VIDEO_X_SIZE];
extern uint32_t __vesacon_shadowfb[VIDEO_Y_SIZE][VIDEO_X_SIZE];

extern unsigned char __vesacon_alpha_tbl[256][4];

extern int __vesacon_init_background(void);
int vesacon_load_background(const char *);

#endif /* LIB_SYS_VESA_VIDEO_H */
