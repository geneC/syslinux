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

#include <stdio.h>
#include <png.h>
#include <com32.h>
#include "vesa.h"
#include "video.h"

/* FIX THIS: we need to redraw any text on the screen... */
static void draw_background(void)
{
  memcpy(__vesa_info.mi.lfb_ptr, __vesacon_background,
	 sizeof __vesacon_background);
}

int vesacon_load_background(const char *filename)
{
  FILE *fp;
  uint8_t header[8];
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_infop end_ptr = NULL;
  png_color_16p image_background;
  static const png_color_16 my_background = {0,0,0,0,0};
  png_bytep row_pointers[VIDEO_Y_SIZE];
  int passes;
  int i;
  int rv = -1;

  if (!filename) {
    draw_background();
    return 0;
  }

  fp = fopen(filename, "r");

  if (!fp)
    goto err;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
				   NULL, NULL, NULL);

  info_ptr = png_create_info_struct(png_ptr);
  end_ptr = png_create_info_struct(png_ptr);

  if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8) ||
      !png_ptr || !info_ptr || !end_ptr ||
      setjmp(png_jmpbuf(png_ptr)))
    goto err;

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);

  png_set_user_limits(png_ptr, VIDEO_X_SIZE, VIDEO_Y_SIZE);

  png_read_info(png_ptr, info_ptr);

  /* Set the appropriate set of transformations.  We need to end up
     with 32-bit BGRA format, no more, no less. */
  
  switch (info_ptr->color_type) {
  case PNG_COLOR_TYPE_GRAY_ALPHA:
    png_set_gray_to_rgb(png_ptr);
    /* fall through */
    
  case PNG_COLOR_TYPE_RGB_ALPHA:
    break;

  case PNG_COLOR_TYPE_GRAY:
    png_set_gray_to_rgb(png_ptr);
    /* fall through */

  case PNG_COLOR_TYPE_RGB:
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
    else
      png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
    break;
    
  case PNG_COLOR_TYPE_PALETTE:
    png_set_palette_to_rgb(png_ptr);
    break;

  default:
    /* Huh? */
    break;
  }

  png_set_bgr(png_ptr);

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
  for (i = 0; i < info_ptr->height; i++)
    row_pointers[i] = (png_bytep *)__vesacon_background[i];
    
  passes = png_set_interlace_handling(png_ptr);

  for (i = 0; i < passes; i++)
    png_read_rows(png_ptr, row_pointers, NULL, info_ptr->height);

  /* This actually displays the stuff */
  draw_background();

  rv = 0;

 err:
  if (png_ptr)
    png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
  if (fp)
    fclose(fp);
  return rv;
}

int __vesacon_init_background(void)
{
  memset(__vesacon_background, 0x80, sizeof __vesacon_background);

  /* The VESA BIOS has already cleared the screen */
  draw_background();
  return 0;
}
