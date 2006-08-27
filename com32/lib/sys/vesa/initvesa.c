/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1999-2006 H. Peter Anvin - All Rights Reserved
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
 * initvesa.c
 *
 * Query the VESA BIOS and select a 640x480x32 mode with local mapping
 * support, if one exists.
 */

#include <inttypes.h>
#include <com32.h>
#include <string.h>
#include "vesa.h"
#include "video.h"

struct vesa_general_info __vesa_general_info;
struct vesa_mode_info    __vesa_mode_info;

uint8_t __vesacon_graphics_font[FONT_MAX_CHARS][FONT_MAX_HEIGHT];
uint32_t __vesacon_background[VIDEO_Y_SIZE][VIDEO_X_SIZE];
uint32_t __vesacon_shadowfb[VIDEO_Y_SIZE][VIDEO_X_SIZE];

static void unpack_font(uint8_t *dst, uint8_t *src, int height)
{
  int i;

  for (i = 0; i < FONT_MAX_CHARS; i++) {
    memcpy(dst, src, height);
    memset(dst+height, 0, 32-height);

    dst += 32;
    src += height;
  }
}

static int vesacon_set_mode(void)
{
  com32sys_t rm;
  uint8_t *rom_font;
  uint16_t mode, *mode_ptr;
  struct vesa_general_info *gi;
  struct vesa_mode_info *mi;

  /* Allocate space in the bounce buffer for these structures */
  gi = &((struct vesa_info *)__com32.cs_bounce)->gi;
  mi = &((struct vesa_info *)__com32.cs_bounce)->mi;

  memset(&rm, 0, sizeof rm);

  gi->signature = VBE2_MAGIC;	/* Get VBE2 extended data */
  rm.eax.w[0] = 0x4F00;		/* Get SVGA general information */
  rm.edi.w[0] = OFFS(gi);
  rm.es      = SEG(gi);
  __intcall(0x10, &rm, &rm);

  if ( rm.eax.w[0] != 0x004F )
    return 1;			/* Function call failed */
  if ( gi->signature != VESA_MAGIC )
    return 2;			/* No magic */
  if ( gi->version < 0x0200 ) {
    return 3;			/* VESA 2.0 not supported */
  }

  /* Search for a 640x480 32-bit linear frame buffer mode */
  mode_ptr = CVT_PTR(gi->video_mode_ptr);

  for(;;) {
    if ((mode = *mode_ptr++) == 0xFFFF)
      return 4;			/* No mode found */

    rm.eax.w[0] = 0x4F01;	/* Get SVGA mode information */
    rm.ecx.w[0] = mode;
    rm.edi.w[0] = OFFS(mi);
    rm.es  = SEG(mi);
    __intcall(0x10, &rm, &rm);

    /* Must be a supported mode */
    if ( rm.eax.w[0] != 0x004f )
      continue;
    /* Must be an LFB color graphics mode supported by the hardware */
    if ( (mi->mode_attr & 0x0099) != 0x0099 )
      continue;
    /* Must be 640x480, 32 bpp */
    if ( mi->h_res != VIDEO_X_SIZE || mi->v_res != VIDEO_Y_SIZE ||
	 mi->bpp != 32 )
      continue;
    /* Must either be a packed-pixel mode or a direct color mode
       (depending on VESA version ) */
    if ( mi->memory_layout != 4 && /* Packed pixel */
	 (mi->memory_layout != 6 || mi->rpos != 24 ||
	  mi->gpos != 16 || mi->bpos != 0) )
      continue;

    /* Hey, looks like we found something we can live with */
    break;
  }

  /* Download the BIOS-provided font */
  rm.eax.w[0] = 0x1130;		/* Get Font Information */
  rm.ebx.w[0] = 0x0600;		/* Get 8x16 ROM font */
  __intcall(0x10, &rm, &rm);
  rom_font = MK_PTR(rm.es, rm.ebp.w[0]);
  unpack_font(graphics_font, rom_font, 16);

  /* Now set video mode */
  rm.eax.w[0] = 0x4F02;		/* Set SVGA video mode */
  rm.ebx.w[0] = mode | 0xC000;	/* Don't clear video RAM, use linear fb */
  __intcall(0x10, &rm, &rm);
  if ( rm.eax.w[0] != 0x004F ) {
    rm.eax.w[0] = 0x0003;	/* Set regular text mode */
    __intcall(0x10, &rm, NULL);
    return 9;			/* Failed to set mode */
  }

  /* Copy established state out of the bounce buffer */
  memcpy(&__vesa_info, __com32.cs_bounce, sizeof __vesa_info);

  return 0;
}

int __vesacon_init(void)
{
  return vesacon_set_mode();
}
