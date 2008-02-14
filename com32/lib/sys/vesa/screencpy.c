/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
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
#include <minmax.h>
#include <klibc/compiler.h>
#include <string.h>
#include <com32.h>
#include "vesa.h"
#include "video.h"

static struct win_info {
  char *win_base;
  size_t win_pos;
  size_t win_size;
  int win_gshift;
  int win_num;
} wi;

static void *
memcpy_to_paged_screen(void *dst, const void *src, size_t len);

static inline int __constfunc ilog2(unsigned int x)
{
  asm("bsrl %1,%0" : "=r" (x) : "rm" (x));
  return x;
}

static void * (*memcpy_to_screen)(void *, const void *, size_t);

void __vesacon_init_copy_to_screen(void)
{
  struct vesa_mode_info * const mi = &__vesa_info.mi;
  int winn;

  if (mi->mode_attr & 0x0080) {
    memcpy_to_screen = memcpy;
  } else {
    memcpy_to_screen = memcpy_to_paged_screen;

    mi->lfb_ptr = 0;		/* Zero-base this */
    wi.win_pos = -1;		/* Undefined position */
    
    /* We have already tested that *one* of these is usable */
    if ((mi->win_attr[0] & 0x05) == 0x05 && mi->win_seg[0])
      winn = 0;
    else
      winn = 1;
    
    wi.win_num    = winn;
    wi.win_base   = (char *)(mi->win_seg[winn] << 4);
    wi.win_size   = mi->win_size << 10;
    wi.win_gshift = ilog2(mi->win_grain) + 10;
  }
}

void __vesacon_copy_to_screen(void *dst, const uint32_t *src, size_t npixels)
{
  size_t bytes = npixels * __vesacon_bytes_per_pixel;
  char rowbuf[bytes+4];

  __vesacon_format_pixels(rowbuf, src, npixels);
  memcpy_to_screen(dst, rowbuf, bytes);
}

static void *
memcpy_to_paged_screen(void *dst, const void *src, size_t len)
{
  size_t win_pos, win_off;
  size_t win_size = wi.win_size;
  size_t omask = win_size - 1;
  char *win_base = wi.win_base;
  size_t l;
  size_t d = (size_t)dst;
  const char *s = src;

  while (len) {
    win_off = d & omask;
    win_pos = d & ~omask;

    if (win_pos != wi.win_pos) {
      com32sys_t ireg;
      memset(&ireg, 0, sizeof ireg);
      ireg.eax.w[0] = 0x4F05;
      ireg.ebx.b[0] = wi.win_num;
      ireg.edx.w[0] = win_pos >> wi.win_gshift;
      __intcall(0x10, &ireg, NULL);
      wi.win_pos = win_pos;
    }

    l = min(len, win_size - win_off);
    memcpy(win_base + win_off, s, l);

    len -= l;
    s += l;
    d += l;
  }

  return dst;
}
