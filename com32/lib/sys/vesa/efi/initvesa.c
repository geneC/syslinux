/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1999-2012 H. Peter Anvin - All Rights Reserved
 *   Chandramouli Narayanan - extended for EFI support
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
#include <stdlib.h>
#include <string.h>
#include <sys/fpu.h>
#include <syslinux/video.h>
#include <dprintf.h>
#ifdef SYSLINUX_EFI
#include <efi.h>
#include <efilib.h>
#include <efistdarg.h>
/* We use cp865_8x16.psf as the standard font for EFI implementation
 * the header file below contains raw data parsed from cp865_8x16.psf
 */
#include "cp865_8x16.h"
#endif
#include "vesa.h"
#include "video.h"
#include "fill.h"
#include "debug.h"

struct vesa_info __vesa_info;

struct vesa_char *__vesacon_text_display;

int __vesacon_font_height;
int __vesacon_text_rows;
int __vesacon_text_cols;
enum vesa_pixel_format __vesacon_pixel_format = PXF_NONE;
unsigned int __vesacon_bytes_per_pixel;
uint8_t __vesacon_graphics_font[FONT_MAX_CHARS][FONT_MAX_HEIGHT];

uint32_t *__vesacon_background, *__vesacon_shadowfb;

static void unpack_font(uint8_t * dst, uint8_t * src, int height)
{
    int i;

    for (i = 0; i < FONT_MAX_CHARS; i++) {
	memcpy(dst, src, height);
	memset(dst + height, 0, FONT_MAX_HEIGHT - height);

	dst += FONT_MAX_HEIGHT;
	src += height;
    }
}

/* EFI GOP support
 * Note GOP support uses the VESA info structure as much as possible and
 * extends it as needed for EFI support. Not all of the vesa info structure
 * is populated. Care must be taken in the routines that rely the vesa
 * informataion structure
 */
static void find_pixmask_bits(uint32_t mask, uint8_t *first_bit, uint8_t *len) {
    uint8_t bit_pos = 0, bit_len = 0;

    *first_bit = 0;
    *len = 0;
    if (mask == 0)
	return;
    while (!(mask & 0x1)) {
	mask = mask >> 1;
	bit_pos++;
    }
    while (mask & 0x1) {
	mask = mask >> 1;
	bit_len++;
    }
    *first_bit = bit_pos;
    *len = bit_len;
}
static int vesacon_set_mode(int *x, int *y)
{
    EFI_GUID GraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput = NULL;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *gop_mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info;
    EFI_STATUS st;
    UINT32 mode_num = 0, bestmode;
    BOOLEAN mode_match = FALSE;
    UINTN sz_info;
    struct vesa_info *vi;
    struct vesa_mode_info *mi;
    enum vesa_pixel_format bestpxf = PXF_NONE;
    int err = 0;

    //debug("Hello, World!\r\n");
    /* At this point, we assume that gnu-efi library is initialized */
    st = LibLocateProtocol(&GraphicsOutputProtocolGuid, (VOID **) &GraphicsOutput);
    if (EFI_ERROR(st)) {
	debug("LiblocateProtocol for GOP failed %d\n", st);
	return 1; /* function call failed */
    }

    /* We use the VESA info structure to store relevant GOP info as much as possible */
    gop_mode = GraphicsOutput->Mode;

    mode_info = gop_mode->Info;
    dprintf("mode %d version %d pixlfmt %d hres=%d vres=%d\n", mode_num, 
			mode_info->Version, mode_info->PixelFormat,
			mode_info->HorizontalResolution, mode_info->VerticalResolution);
    
    /* simply pick the best mode that suits the caller's resolution */
    for (mode_num = 0; mode_num < gop_mode->MaxMode; mode_num++) {
	st = uefi_call_wrapper(GraphicsOutput->QueryMode, 4, GraphicsOutput, mode_num, &sz_info, &mode_info);
	debug("mode_num = %d query_status %d\n", mode_num, st);
	if (st == EFI_SUCCESS && sz_info >= sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION)) {

		/* For now, simply pick the best mode that suits caller's resolution (x,y)
		 * FIXME: Consider any additional criteria for matching mode
		 */
		mode_match = ((uint32_t)*x == mode_info->HorizontalResolution && (uint32_t)*y == mode_info->VerticalResolution);
		debug("mode %d hres=%d vres=%d\n", mode_num, mode_info->HorizontalResolution, mode_info->VerticalResolution);
		if (mode_match) {
			bestmode = mode_num;
			break;
		}
	}
    }

    if (!mode_match) {
	/* Instead of bailing out, set the mode to the system default.
 	 * Some systems do not have support for 640x480 for instance
 	 * This code deals with such cases.
 	 */
	mode_info = gop_mode->Info;
	*x = mode_info->HorizontalResolution;
	*y = mode_info->VerticalResolution;
	bestmode = gop_mode->Mode;
	debug("No matching mode, setting to available default mode %d (x=%d, y=%d)\n", bestmode, *x, *y);
    }

    /* Allocate space in the bounce buffer for these structures */
    vi = malloc(sizeof(*vi));
    if (!vi) {
	err = 10;		/* Out of memory */
	goto exit;
    }
    /* Note that the generic info is untouched as we don't find any relevance to EFI */
    mi = &vi->mi;
    /* Set up mode-specific information */
    mi->h_res = *x;
    mi->v_res = *y;
    mi->lfb_ptr = (uint8_t *)(VOID *)(UINTN)gop_mode->FrameBufferBase;
    mi->lfb_size = gop_mode->FrameBufferSize;

    /* FIXME: 
     * The code below treats bpp == lfb_depth ; verify
     */

    switch (mode_info->PixelFormat) {
    case PixelRedGreenBlueReserved8BitPerColor:
	dprintf("RGB8bit ");
	mi->mode_attr = 0x0080;		/* supports physical frame buffer */
	mi->bpp = sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * 8;
	mi->rpos = 0;
	mi->gpos = 8;
	mi->bpos = 16;
	mi->resv_pos = 24;
	mi->resv_size = 8;
	mi->logical_scan = mi->lfb_line_size = (mode_info->PixelsPerScanLine * mi->bpp) / 8;
	bestpxf = PXF_BGRA32;
	dprintf("bpp %d pixperScanLine %d logical_scan %d bytesperPix %d\n", mi->bpp, mode_info->PixelsPerScanLine, 
		mi->logical_scan, (mi->bpp + 7)>>3);
	break;
    case PixelBlueGreenRedReserved8BitPerColor:
	dprintf("BGR8bit ");
	mi->mode_attr = 0x0080;		/* supports physical frame buffer */
	mi->bpp = sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * 8;
	mi->bpos = 0;
	mi->gpos = 8;
	mi->rpos = 16;
	mi->resv_pos = 24;
	mi->resv_size = 8;
	mi->logical_scan = mi->lfb_line_size = (mode_info->PixelsPerScanLine * mi->bpp) / 8;
	bestpxf = PXF_BGRA32;
	dprintf("bpp %d pixperScanLine %d logical_scan %d bytesperPix %d\n", mi->bpp, mode_info->PixelsPerScanLine, 
		mi->logical_scan, (mi->bpp + 7)>>3);
	break;
    case PixelBitMask:
	mi->mode_attr = 0x0080;		/* supports physical frame buffer */
	dprintf("RedMask 0x%x GrnMask 0x%x BluMask 0x%x RsvMask 0x%x\n",
		mode_info->PixelInformation.RedMask,
		mode_info->PixelInformation.GreenMask,
		mode_info->PixelInformation.BlueMask,
		mode_info->PixelInformation.ReservedMask);
	find_pixmask_bits(mode_info->PixelInformation.RedMask,
                          &mi->rpos, &mi->lfb_rsize);
	find_pixmask_bits(mode_info->PixelInformation.GreenMask,
                          &mi->gpos, &mi->lfb_gsize);
	find_pixmask_bits(mode_info->PixelInformation.BlueMask,
                          &mi->bpos, &mi->lfb_bsize);
	find_pixmask_bits(mode_info->PixelInformation.ReservedMask,
                          &mi->resv_pos, &mi->lfb_resv_size);
	mi->bpp = mi->lfb_rsize + mi->lfb_gsize +
                                  mi->lfb_bsize + mi->lfb_resv_size;
	mi->logical_scan = mi->lfb_line_size = (mode_info->PixelsPerScanLine * mi->bpp) / 8;
	dprintf("RPos %d Rsize %d GPos %d Gsize %d\n", mi->rpos, mi->lfb_rsize, mi->gpos, mi->lfb_gsize);
	dprintf("BPos %d Bsize %d RsvP %d RsvSz %d\n", mi->bpos, mi->lfb_bsize, mi->resv_pos, mi->lfb_resv_size);
	dprintf("bpp %d logical_scan %d bytesperPix %d\n", mi->bpp, mi->logical_scan, (mi->bpp + 7)>>3);
	switch (mi->bpp) {
	case 32:
		bestpxf = PXF_BGRA32;
		break;
	case 24:
		bestpxf = PXF_BGR24;
		break;
	case 16:
		bestpxf = PXF_LE_RGB16_565;
		break;
	default:
		dprintf("Unable to handle bits per pixel %d, bailing out\n", mi->bpp);
		err = 4;
		goto exit;
	}
	break;
    case PixelBltOnly:
	/* FIXME: unsupported */
	mi->mode_attr = 0x0000;		/* no support for physical frame buffer */
	err = 4; /* no mode found */
	goto exit;
	break;
    default:
	/* should not get here, but let's error out */
	err = 4; /* no mode found */
	goto exit;
	break;
    }		   
    
    memcpy(&__vesa_info.mi, mi, sizeof *mi);
    mi = &__vesa_info.mi;
    /* set up font info
     * For now, font info is stored as raw data and used
     * as such. Altenatively, the font data stored in a file 
     * could be read and parsed. (note: for this, EFI
     * file support should be exposed via firmware structure)
     */
    __vesacon_font_height = cp865_8x16_font_height;
    unpack_font((uint8_t *) __vesacon_graphics_font,
		(uint8_t *)cp865_8x16_font_data,
		__vesacon_font_height);
   
    /* Free any existing data structures */
    if (__vesacon_background) {
	free(__vesacon_background);
	__vesacon_background = NULL;
    }
    if (__vesacon_shadowfb) {
	free(__vesacon_shadowfb);
	__vesacon_shadowfb = NULL;
    }

    __vesacon_bytes_per_pixel = (mi->bpp + 7) >> 3;
    __vesacon_format_pixels = __vesacon_format_pixels_list[bestpxf];

    /* Now set video mode */
    st = uefi_call_wrapper(GraphicsOutput->SetMode, 2, GraphicsOutput, bestmode);
    if (EFI_ERROR(st)) {
	err = 9;		/* Failed to set mode */
	dprintf("Failed to set mode %d\n", bestmode);
	goto exit;
    }	

    /* TODO: Follow the code usage of vesacon_background & vesacon_shadowfb */
    /*
     __vesacon_background = calloc(mi->h_res*mi->v_res, 4);
     __vesacon_shadowfb = calloc(mi->h_res*mi->v_res, 4);
     */
     /* FIXME: the allocation takes the possible padding into account
      * whereas   BIOS code simply allocates hres * vres bytes.
      * Which is correct?
      */
     /*
      * For performance reasons, or due to hardware restrictions, scan lines
      * may be padded to an amount of memory alignment. These padding pixel elements
      * are outside the area covered by HorizontalResolution and are not visible.
      * For direct frame buffer access, this number is used as a span between starts
      * of pixel lines in video memory. Based on the size of an individual pixel element
      * and PixelsPerScanline, the offset in video memory from pixel element (x, y)
      * to pixel element (x, y+1) has to be calculated as 
      * "sizeof( PixelElement ) * PixelsPerScanLine", and not 
      * "sizeof( PixelElement ) * HorizontalResolution", though in many cases
      * those values can coincide.
      */

    __vesacon_background = calloc(mi->lfb_line_size*mi->v_res, 4);
    __vesacon_shadowfb = calloc(mi->lfb_line_size*mi->v_res, 4);

    __vesacon_init_copy_to_screen();

    /* Tell syslinux we changed video mode
     * FIXME: The following call is BIOS-centric. I don't see an EFI-equivalent
     * syslinux_report_video_mode(0x000f, mi->h_res, mi->v_res);
     */

    __vesacon_pixel_format = bestpxf;

exit:
    if (vi)
	free(vi);

    return err;
}

/* FIXME: 
 * Does init_text_display need an EFI counterpart?
 * e.g. vesa_char may need to setup UNICODE char for EFI
 * and the number of screen chars may need to be sized up
 * accordingly. This may also require turning byte strings
 * into unicode strings in the framebuffer
 * Possibly, revisit vesacon_fill() for EFI.
 */

static int init_text_display(void)
{
    size_t nchars;
    struct vesa_char *ptr;
    struct vesa_char def_char = {
	.ch = ' ',
	.attr = 0,
    };

    if (__vesacon_text_display)
	free(__vesacon_text_display);

    __vesacon_text_cols = TEXT_PIXEL_COLS / FONT_WIDTH;
    __vesacon_text_rows = TEXT_PIXEL_ROWS / __vesacon_font_height;
    nchars = (__vesacon_text_cols+2) * (__vesacon_text_rows+2);

    __vesacon_text_display = ptr = malloc(nchars * sizeof(struct vesa_char));

    if (!ptr)
	return -1;

    vesacon_fill(ptr, def_char, nchars);
    __vesacon_init_cursor(__vesacon_font_height);

    return 0;
}

/* On input, VESA initialization is passed a desirable resolution
 * On return, either the requested resolution is set or the system
 * supported default resolution is set and returned to the caller
 * This change is added for EFI enabled platforms.
 */
int __vesacon_init(int *x, int *y)
{
    /* We need the FPU for graphics, at least libpng et al will need it... */
    if (x86_init_fpu())
	return 10;

    vesacon_set_mode(x, y);
    /* FIXME: Accessing Video BIOS from EFI will probably not work, skip it for now */

    init_text_display();

    debug("Mode set, now drawing at %#p\r\n", __vesa_info.mi.lfb_ptr);

    __vesacon_init_background();

    debug("Ready!\r\n");
    return 0;
}
