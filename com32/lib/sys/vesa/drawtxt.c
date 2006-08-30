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

#include <inttypes.h>
#include "vesa.h"
#include "video.h"

/*
 * Color map (ARGB) for the 16 PC colors.  This can be overridden by the
 * application, and do not have to match the traditional colors.
 */
uint32_t vesacon_color_map[16] = {
  0x00000000,			/* black */
  0x400000ff,			/* dark blue */
  0x4000ff00,			/* dark green */
  0x4000ffff,			/* dark cyan */
  0x40ff0000,			/* dark red */
  0x40ff00ff,			/* dark purple */
  0x80ff8000,			/* brown */
  0xc0ffffff,			/* light grey */
  0x40ffffff,			/* dark grey */
  0xc00000ff,			/* bright blue */
  0xc000ff00,			/* bright green */
  0xc000ffff,			/* bright cyan */
  0xc0ff0000,			/* bright red */
  0xc0ff00ff,			/* bright purple */
  0xffffff00,			/* yellow */
  0xffffffff,			/* white */
};

/*
 * Linear alpha blending.  Useless for anything that's actually
 * depends on color accuracy (because of gamma), but it's fine for
 * what we want.
 *
 * This algorithm is exactly equivalent to (alpha*fg+(255-alpha)*bg)/255
 * for all 8-bit values, but is substantially faster.
 */
static inline __attribute__((always_inline))
  uint8_t alpha_val(uint8_t fg, uint8_t bg, uint8_t alpha)
{
  unsigned int tmp = 1 + alpha*fg + (255-alpha)*bg;
  return (tmp + (tmp >> 8)) >> 8;
}

static uint32_t alpha_pixel(uint32_t fg, uint32_t bg)
{
  uint8_t alpha = fg >> 24;
  uint8_t fg_r = fg >> 16;
  uint8_t fg_g = fg >> 8;
  uint8_t fg_b = fg;
  uint8_t bg_r = bg >> 16;
  uint8_t bg_g = bg >> 8;
  uint8_t bg_b = bg;

  return
    (alpha_val(fg_r, bg_r, alpha) << 16)|
    (alpha_val(fg_g, bg_g, alpha) << 8)|
    (alpha_val(fg_b, bg_b, alpha));
}

static void vesacon_update_characters(int row, int col, int nrows, int ncols)
{
  const int height = __vesacon_font_height;
  const int width = FONT_WIDTH;
  uint32_t *bgrowptr, *bgptr, *fbrowptr, *fbptr, bgval, fgval;
  uint32_t fgcolor = 0, bgcolor = 0, color;
  uint8_t chbits = 0, chxbits = 0, chsbits = 0;
  int i, j, pixrow, pixsrow;
  struct vesa_char *rowptr, *rowsptr, *cptr, *csptr;

  bgrowptr  = &__vesacon_background[row*height+VIDEO_BORDER][col*width+VIDEO_BORDER];
  fbrowptr = ((uint32_t *)__vesa_info.mi.lfb_ptr)+
    ((row*height+VIDEO_BORDER)*VIDEO_X_SIZE)+(col*width+VIDEO_BORDER);

  /* Note that we keep a 1-character guard area around the real text area... */
  rowptr  = &__vesacon_text_display[(row+1)*(TEXT_PIXEL_COLS/FONT_WIDTH+2)+(col+1)];
  rowsptr = rowptr - ((TEXT_PIXEL_COLS/FONT_WIDTH+2)+1);
  pixrow = 0;
  pixsrow = height-1;

  for (i = height*nrows; i >= 0; i--) {
    bgptr = bgrowptr;
    fbptr = fbrowptr;

    cptr = rowptr;
    csptr = rowsptr;

    chsbits = __vesacon_graphics_font[csptr->ch][pixsrow];
    chsbits &= (csptr->sha & 0x02) ? 0xff : 0x00;
    chsbits ^= (csptr->sha & 0x01) ? 0xff : 0x00;
    chsbits <<= 7;
    csptr++;
    
    for (j = width*ncols; j >= 0; j--) {
      chbits <<= 1;
      chsbits <<= 1;
      chxbits <<= 1;

      switch (j % FONT_WIDTH) {
      case 0:
	chbits = __vesacon_graphics_font[cptr->ch][pixrow];
	chxbits = chbits;
	chxbits &= (cptr->sha & 0x02) ? 0xff : 0x00;
	chxbits ^= (cptr->sha & 0x01) ? 0xff : 0x00;
	fgcolor = vesacon_color_map[cptr->sha & 0x0f];
	bgcolor = vesacon_color_map[cptr->sha >> 4];
	cptr++;
	break;
      case FONT_WIDTH-1:
	chsbits = __vesacon_graphics_font[csptr->ch][pixsrow];
	chsbits &= (csptr->sha & 0x02) ? 0xff : 0x00;
	chsbits ^= (csptr->sha & 0x01) ? 0xff : 0x00;
	csptr++;
	break;
      default:
	break;
      }

      /* If this pixel is raised, use the offsetted value */
      bgval = (chxbits & 0x80) ? bgptr[VIDEO_X_SIZE+1] : *bgptr;
      bgptr++;

      /* If this pixel is set, use the fg color, else the bg color */
      fgval = (chbits & 0x80) ? fgcolor : bgcolor;

      /* Produce the combined color pixel value */
      color = alpha_pixel(fgval, bgval);

      /* Apply the shadow (75% shadow) */
      if ((chsbits & ~chxbits) & 0x80) {
	color >>= 2;
	color &= 0x3f3f3f;
      }
      
      *fbptr++ = color;
    }

    bgrowptr += VIDEO_X_SIZE;
    fbrowptr += VIDEO_X_SIZE;

    if (++pixrow == height) {
      rowptr += TEXT_PIXEL_COLS/FONT_WIDTH+2;
      pixrow = 0;
    }
    if (++pixsrow == height) {
      rowsptr += TEXT_PIXEL_COLS/FONT_WIDTH+2;
      pixsrow = 0;
    }
  }
}

void vesacon_write_at(int row, int col, const char *str, uint8_t attr, int rev)
{
  int n = 0;
  struct vesa_char *ptr = &__vesacon_text_display[(row+1)*(TEXT_PIXEL_COLS/FONT_WIDTH+2)+(col+1)];

  while (*str) {
    ptr->ch   = *str;
    ptr->attr = attr;
    ptr->sha  = rev;

    n++;
    str++;
    ptr++;
  }

  vesacon_update_characters(row, col, 1, n);
}

