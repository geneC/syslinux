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

/*
 * fmtpixel.h
 *
 * Inline function to format a single pixel
 */

#ifndef LIB_SYS_VESA_FMTPIXEL_H
#define LIB_SYS_VESA_FMTPIXEL_H

#include <inttypes.h>
#include "video.h"

/* Format a pixel and return the advanced pointer.
   THIS FUNCTION IS ALLOWED TO WRITE BEYOND THE END OF THE PIXEL. */

static inline void *format_pixel(void *ptr, uint32_t rgba,
				 enum vesa_pixel_format fmt)
{
  switch (fmt) {
  case PXF_BGRA32:
    *(uint32_t *)ptr = rgba;
    return (uint32_t *)ptr + 1;

  case PXF_BGR24:
    *(uint32_t *)ptr = rgba;
    return (uint8_t *)ptr + 3;
    
  case PXF_LE_RGB16_565:
    {
      uint16_t pxv =
	((rgba >> 3) & 0x1f) |
	((rgba >> (2+8-5)) & (0x3f << 5)) |
	((rgba >> (3+16-11)) & (0x1f << 11));

      *(uint16_t *)ptr = pxv;
      return (uint16_t *)ptr + 1;
    }

  case PXF_NONE:		/* Shuts up gcc */
    return ptr;
  }
}

#endif /* LIB_SYS_VESA_FMTPIXEL_H */
