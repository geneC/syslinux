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
 * Linear alpha blending.  Useless for anything that's actually
 * depends on color accuracy (because of gamma), but it's fine for
 * what we want.
 *
 * This algorithm is exactly equivalent to (alpha*fg+(255-alpha)*bg)/255
 * for all 8-bit values, but is substantially faster.
 */
static inline uint8_t alpha(uint8_t fg, uint8_t bg, uint8_t alpha)
{
  unsigned int tmp = 1 + alpha*fg + (255-alpha)*bg;
  return (tmp + (tmp >> 8)) >> 8;
}

static void vesacon_update_characters(int row, int col, int nrows, int ncols)
{
  const int height = __vesacon_font_height;
  const int width = FONT_WIDTH;
  uint32_t *bgrowptr, *bgptr, *fbrowptr, *fbptr, bgval, bgxval;
  int fr = 0, fg = 0, fb = 0;
  int br = 0, bg = 0, bb = 0;
  uint8_t r, g, b;
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
	fr = ((cptr->attr & 4) >> 1) + ((cptr->attr & 8) >> 3);
	fg = ((cptr->attr & 2)) + ((cptr->attr & 8) >> 3);
	fb = ((cptr->attr & 1) << 1) + ((cptr->attr & 8) >> 3);
	br = ((cptr->attr & 0x40) >> 5) + ((cptr->attr & 0x80) >> 7);
	bg = ((cptr->attr & 0x20) >> 4) + ((cptr->attr & 0x80) >> 7);
	bb = ((cptr->attr & 0x10) >> 3) + ((cptr->attr & 0x80) >> 7);
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

      bgval = *bgptr;
      bgxval = bgptr[VIDEO_X_SIZE+1];
      bgptr++;
      
      if (chbits & 0x80) {
	r = __vesacon_alpha_tbl[(uint8_t)(bgxval >> 16)][fr];
	g = __vesacon_alpha_tbl[(uint8_t)(bgxval >> 8)][fg];
	b = __vesacon_alpha_tbl[(uint8_t)bgxval][fb];
      } else if ((chsbits & ~chxbits) & 0x80) {
	r = __vesacon_alpha_tbl[(uint8_t)(bgval >> 16)][br] >> 2;
	g = __vesacon_alpha_tbl[(uint8_t)(bgval >> 8)][bg] >> 2;
	b = __vesacon_alpha_tbl[(uint8_t)bgval][bb] >> 2;
      } else {
	r = __vesacon_alpha_tbl[(uint8_t)(bgval >> 16)][br];
	g = __vesacon_alpha_tbl[(uint8_t)(bgval >> 8)][bg];
	b = __vesacon_alpha_tbl[(uint8_t)bgval][bb];
      }
      
      *fbptr++ = (r << 16)|(g << 8)|b;
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

