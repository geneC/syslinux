/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006-2008 H. Peter Anvin - All Rights Reserved
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

#include <stdio.h>
#include <png.h>
#include <tinyjpeg.h>
#include <com32.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <minmax.h>
#include <stdbool.h>
#include <syslinux/loadfile.h>
#include "vesa.h"
#include "video.h"

/*** FIX: This really should be alpha-blended with color index 0 ***/

/* For best performance, "start" should be a multiple of 4, to assure
   aligned dwords. */
static void draw_background_line(int line, int start, int npixels)
{
  uint32_t *bgptr = &__vesacon_background[line][start];
  unsigned int bytes_per_pixel = __vesacon_bytes_per_pixel;
  size_t fbptr = line*__vesa_info.mi.logical_scan + start*bytes_per_pixel;

  __vesacon_copy_to_screen(fbptr, bgptr, npixels);
}

/* This draws the border, then redraws the text area */
static void draw_background(void)
{
  int i;
  const int bottom_border = VIDEO_BORDER +
    (TEXT_PIXEL_ROWS % __vesacon_font_height);
  const int right_border = VIDEO_BORDER + (TEXT_PIXEL_COLS % FONT_WIDTH);

  for (i = 0; i < VIDEO_BORDER; i++)
    draw_background_line(i, 0, VIDEO_X_SIZE);

  for (i = VIDEO_BORDER; i < VIDEO_Y_SIZE-bottom_border; i++) {
    draw_background_line(i, 0, VIDEO_BORDER);
    draw_background_line(i, VIDEO_X_SIZE-right_border, right_border);
  }

  for (i = VIDEO_Y_SIZE-bottom_border; i < VIDEO_Y_SIZE; i++)
    draw_background_line(i, 0, VIDEO_X_SIZE);

  __vesacon_redraw_text();
}

static int read_png_file(FILE *fp)
{
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_infop end_ptr = NULL;
#if 0
  png_color_16p image_background;
  static const png_color_16 my_background = {0,0,0,0,0};
#endif
  png_bytep row_pointers[VIDEO_Y_SIZE];
  int passes;
  int i;
  int rv = -1;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
				   NULL, NULL, NULL);

  info_ptr = png_create_info_struct(png_ptr);
  end_ptr = png_create_info_struct(png_ptr);

  if (!png_ptr || !info_ptr || !end_ptr ||
      setjmp(png_jmpbuf(png_ptr)))
    goto err;

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);

  png_set_user_limits(png_ptr, VIDEO_X_SIZE, VIDEO_Y_SIZE);

  png_read_info(png_ptr, info_ptr);

  /* Set the appropriate set of transformations.  We need to end up
     with 32-bit BGRA format, no more, no less. */

  /* Expand to RGB first... */
  if (info_ptr->color_type & PNG_COLOR_MASK_PALETTE)
    png_set_palette_to_rgb(png_ptr);
  else if (!(info_ptr->color_type & PNG_COLOR_MASK_COLOR))
    png_set_gray_to_rgb(png_ptr);

  /* Add alpha channel, if need be */
  if (!(png_ptr->color_type & PNG_COLOR_MASK_ALPHA)) {
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
    else
      png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
  }

  /* Adjust the byte order, if necessary */
  png_set_bgr(png_ptr);

  /* Make sure we end up with 8-bit data */
  if (info_ptr->bit_depth == 16)
    png_set_strip_16(png_ptr);
  else if (info_ptr->bit_depth < 8)
    png_set_packing(png_ptr);

#if 0
  if (png_get_bKGD(png_ptr, info_ptr, &image_background))
    png_set_background(png_ptr, image_background,
		       PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
  else
    png_set_background(png_ptr, &my_background,
		       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
#endif

  /* Whew!  Now we should get the stuff we want... */
  for (i = 0; i < (int)info_ptr->height; i++)
    row_pointers[i] = (void *)__vesacon_background[i];

  passes = png_set_interlace_handling(png_ptr);

  for (i = 0; i < passes; i++)
    png_read_rows(png_ptr, row_pointers, NULL, info_ptr->height);

  rv = 0;

 err:
  if (png_ptr)
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
  return rv;
}

static int jpeg_sig_cmp(uint8_t *bytes, int len)
{
  (void)len;
  return (bytes[0] == 0xff && bytes[1] == 0xd8) ? 0 : -1;
}

static int read_jpeg_file(FILE *fp, uint8_t *header, int len)
{
  struct jdec_private *jdec = NULL;
  void *jpeg_file = NULL;
  size_t length_of_file;
  unsigned int width, height;
  int rv = -1;
  unsigned char *components[1];
  unsigned int bytes_per_row[1];

  rv = floadfile(fp, &jpeg_file, &length_of_file, header, len);
  fclose(fp);
  if (rv)
    goto err;

  jdec = tinyjpeg_init();
  if (!jdec)
    goto err;

  if (tinyjpeg_parse_header(jdec, jpeg_file, length_of_file) < 0)
    goto err;

  tinyjpeg_get_size(jdec, &width, &height);
  if (width > VIDEO_X_SIZE || height > VIDEO_Y_SIZE)
    goto err;

  components[0] = (void *)&__vesacon_background[0];
  tinyjpeg_set_components(jdec, components, 1);
  bytes_per_row[0] = VIDEO_X_SIZE << 2;
  tinyjpeg_set_bytes_per_row(jdec, bytes_per_row, 1);

  tinyjpeg_decode(jdec, TINYJPEG_FMT_BGRA32);

  rv = 0;

 err:
  /* Don't use tinyjpeg_free() here, since we didn't allow tinyjpeg
     to allocate the frame buffer */
  if (jdec)
    free(jdec);

  if (jpeg_file)
    free(jpeg_file);

  return rv;
}

/* Simple grey Gaussian hole, enough to look interesting */
int vesacon_default_background(void)
{
  int x, y, dx, dy, dy2;
  uint8_t *bgptr = (uint8_t *)&__vesacon_background;
  uint8_t k;

  if (__vesacon_pixel_format == PXF_NONE)
    return 0;			/* Not in graphics mode */

  for (y = 0, dy = -VIDEO_Y_SIZE/2; y < VIDEO_Y_SIZE; y++, dy++) {
    dy2 = dy*dy;
    for (x = 0, dx = -VIDEO_X_SIZE/2; x < VIDEO_X_SIZE; x++, dx++) {
      k = __vesacon_linear_to_srgb[500+((dx*dx+dy2) >> 6)];
      bgptr[0] = k;		/* Blue */
      bgptr[1] = k;		/* Green */
      bgptr[2] = k;		/* Red */
      bgptr += 4;		/* Dummy alpha */
    }
  }

  draw_background();
  return 0;
}

/* Set the background to a single flat color */
int vesacon_set_background(unsigned int rgb)
{
  void *bgptr = __vesacon_background;
  unsigned int count = VIDEO_X_SIZE*VIDEO_Y_SIZE;

  if (__vesacon_pixel_format == PXF_NONE)
    return 0;			/* Not in graphics mode */

  asm volatile("rep; stosl"
	       : "+D" (bgptr), "+c" (count)
	       : "a" (rgb)
	       : "memory");

  draw_background();
  return 0;
}

struct lss16_header {
  uint32_t magic;
  uint16_t xsize;
  uint16_t ysize;
};

#define LSS16_MAGIC 0x1413f33d

static inline int lss16_sig_cmp(const void *header, int len)
{
  const struct lss16_header *h = header;

  if (len != 8)
    return 1;

  return !(h->magic == LSS16_MAGIC &&
	   h->xsize <= VIDEO_X_SIZE && h->ysize <= VIDEO_Y_SIZE);
}

static int read_lss16_file(FILE *fp, const void *header, int header_len)
{
  const struct lss16_header *h = header;
  uint32_t colors[16], color;
  bool has_nybble;
  uint8_t byte;
  int count;
  int nybble, prev;
  enum state {
    st_start,
    st_c0,
    st_c1,
    st_c2,
  } state;
  int i, x, y;
  uint32_t *bgptr = (void *)__vesacon_background;

  /* Assume the header, 8 bytes, has already been loaded. */
  if (header_len != 8)
    return -1;

  for (i = 0; i < 16; i++) {
    uint8_t rgb[3];
    if (fread(rgb, 1, 3, fp) != 3)
      return -1;

    colors[i] = (((rgb[0] & 63)*255/63) << 16) +
      (((rgb[1] & 63)*255/63) << 8) +
      ((rgb[2] & 63)*255/63);
  }

  /* By spec, the state machine is per row */
  for (y = 0; y < h->ysize; y++) {
    state = st_start;
    has_nybble = false;
    color = colors[prev = 0];	/* By specification */
    count = 0;

    x = 0;
    while (x < h->xsize) {
      if (!has_nybble) {
	if (fread(&byte, 1, 1, fp) != 1)
	  return -1;
	nybble = byte & 0xf;
	has_nybble = true;
      } else {
	nybble = byte >> 4;
	has_nybble = false;
      }

      switch (state) {
      case st_start:
	if (nybble != prev) {
	  *bgptr++ = color = colors[prev = nybble];
	  x++;
	} else {
	  state = st_c0;
	}
	break;

      case st_c0:
	if (nybble == 0) {
	  state = st_c1;
	} else {
	  count = nybble;
	  goto do_run;
	}
	break;

      case st_c1:
	count = nybble + 16;
	state = st_c2;
	break;

      case st_c2:
	count += nybble << 4;
	goto do_run;

      do_run:
	count = min(count, h->xsize-x);
	x += count;
	asm volatile("rep; stosl"
		     : "+D" (bgptr), "+c" (count) : "a" (color));
	state = st_start;
	break;
      }
    }

    /* Zero-fill rest of row */
    i = VIDEO_X_SIZE-x;
    asm volatile("rep; stosl"
	       : "+D" (bgptr), "+c" (i) : "a" (0) : "memory");
  }

  /* Zero-fill rest of screen */
  i = (VIDEO_Y_SIZE-y)*VIDEO_X_SIZE;
  asm volatile("rep; stosl"
	       : "+D" (bgptr), "+c" (i) : "a" (0) : "memory");

  return 0;
}

int vesacon_load_background(const char *filename)
{
  FILE *fp = NULL;
  uint8_t header[8];
  int rv = 1;

  if (__vesacon_pixel_format == PXF_NONE)
    return 0;			/* Not in graphics mode */

  fp = fopen(filename, "r");

  if (!fp)
    goto err;

  if (fread(header, 1, 8, fp) != 8)
    goto err;

  if (!png_sig_cmp(header, 0, 8)) {
    rv = read_png_file(fp);
  } else if (!jpeg_sig_cmp(header, 8)) {
    rv = read_jpeg_file(fp, header, 8);
  } else if (!lss16_sig_cmp(header, 8)) {
    rv = read_lss16_file(fp, header, 8);
  }

  /* This actually displays the stuff */
  draw_background();

 err:
  if (fp)
    fclose(fp);

  return rv;
}

int __vesacon_init_background(void)
{
  /* The BSS clearing has already cleared __vesacon_background */

  /* The VESA BIOS has already cleared the screen */
  return 0;
}
