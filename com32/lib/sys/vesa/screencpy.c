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
#include <ilog2.h>
#include "vesa.h"
#include "video.h"

static struct win_info {
    char *win_base;
    size_t win_pos;
    size_t win_size;
    int win_gshift;
    int win_num;
} wi;

void __vesacon_init_copy_to_screen(void)
{
    struct vesa_mode_info *const mi = &__vesa_info.mi;
    int winn;

    if (mi->mode_attr & 0x0080) {
	/* Linear frame buffer */

	wi.win_base = (char *)mi->lfb_ptr;
	wi.win_size = 1 << 31;	/* 2 GB, i.e. one huge window */
	wi.win_pos = 0;		/* Already positioned (only one position...) */
	wi.win_num = -1;	/* Not a window */
    } else {
	/* Paged frame buffer */

	/* We have already tested that *one* of these is usable */
	if ((mi->win_attr[0] & 0x05) == 0x05 && mi->win_seg[0])
	    winn = 0;
	else
	    winn = 1;

	wi.win_num = winn;
	wi.win_base = (char *)(mi->win_seg[winn] << 4);
	wi.win_size = mi->win_size << 10;
	wi.win_gshift = ilog2(mi->win_grain) + 10;
	wi.win_pos = -1;	/* Undefined position */
    }
}

static void set_window_pos(size_t win_pos)
{
    static com32sys_t ireg;

    wi.win_pos = win_pos;

    if (wi.win_num < 0)
	return;			/* This should never happen... */

    ireg.eax.w[0] = 0x4F05;
    ireg.ebx.b[0] = wi.win_num;
    ireg.edx.w[0] = win_pos >> wi.win_gshift;

    __intcall(0x10, &ireg, NULL);
}

void __vesacon_copy_to_screen(size_t dst, const uint32_t * src, size_t npixels)
{
    size_t win_pos, win_off;
    size_t win_size = wi.win_size;
    size_t omask = win_size - 1;
    char *win_base = wi.win_base;
    size_t l;
    size_t bytes = npixels * __vesacon_bytes_per_pixel;
    char rowbuf[bytes + 4] __aligned(4);
    const char *s;

    s = (const char *)__vesacon_format_pixels(rowbuf, src, npixels);

    while (bytes) {
	win_off = dst & omask;
	win_pos = dst & ~omask;

	if (__unlikely(win_pos != wi.win_pos))
	    set_window_pos(win_pos);

	l = min(bytes, win_size - win_off);
	memcpy(win_base + win_off, s, l);

	bytes -= l;
	s += l;
	dst += l;
    }
}
