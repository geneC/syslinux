#include <syslinux/linux.h>
#include "efi.h"

extern EFI_GUID GraphicsOutputProtocol;

void writechr(char data)
{
	Print(L"Wanted to print something\n");
}

static inline EFI_STATUS open_protocol(EFI_HANDLE handle, EFI_GUID *protocol,
				       void **interface, EFI_HANDLE agent,
				       EFI_HANDLE controller, UINT32 attributes)
{
	return uefi_call_wrapper(BS->OpenProtocol, 6, handle, protocol,
				 interface, agent, controller, attributes);
}

static inline EFI_STATUS
gop_query_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, UINTN *size,
	       EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **info)
{
	return uefi_call_wrapper(gop->QueryMode, 4, gop,
				 gop->Mode->Mode, size, info);
}

static inline void bit_mask(uint32_t mask, uint8_t *pos, uint8_t *size)
{
	*pos = 0;
	*size = 0;

	if (mask) {
		while (!(mask & 0x1)) {
			mask >>= 1;
			(*pos)++;
		}

		while (mask & 0x1) {
			mask >>= 1;
			(*size)++;
		}
	}
}

void setup_screen(struct screen_info *si)
{
	EFI_HANDLE *handles = NULL;
	EFI_STATUS status;
	UINTN nr_handles;
	UINTN size;
	uint16_t lfb_width, lfb_height;
	uint32_t lfb_base, lfb_size;
	int i;

	status = LibLocateHandle(ByProtocol, &GraphicsOutputProtocol,
				 NULL, &nr_handles, &handles);
	if (status == EFI_BUFFER_TOO_SMALL) {
		EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, *found;
		EFI_GRAPHICS_PIXEL_FORMAT pixel_fmt;
		EFI_PIXEL_BITMASK pixel_info;
		uint32_t pixel_scanline;

		handles = AllocatePool(nr_handles);
		if (!handles)
			return;

		status = LibLocateHandle(ByProtocol, &GraphicsOutputProtocol,
					 NULL, &nr_handles, &handles);
		if (status != EFI_SUCCESS)
			goto out;

		found = NULL;
		for (i = 0; i < (nr_handles / sizeof(EFI_HANDLE)); i++) {
			EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
			EFI_PCI_IO *pciio = NULL;
			EFI_HANDLE *h = handles[i];

			status = open_protocol(h, &GraphicsOutputProtocol,
					       (void **)&gop,
					       image_handle, NULL,
					       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
			if (status != EFI_SUCCESS)
				continue;

			status = open_protocol(h, &PciIoProtocol,
					       (void **)&pciio,
					       image_handle, NULL,
					       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
			status = gop_query_mode(gop, &size, &info);
			if (status != EFI_SUCCESS)
				continue;

			if (!pciio && found)
				continue;
			found = gop;

			lfb_width = info->HorizontalResolution;
			lfb_height = info->VerticalResolution;
			lfb_base = gop->Mode->FrameBufferBase;
			lfb_size = gop->Mode->FrameBufferSize;
			pixel_fmt = info->PixelFormat;
			pixel_info = info->PixelInformation;
			pixel_scanline = info->PixelsPerScanLine;

			if (pciio)
				break;
		}

		if (!found)
			goto out;

		si->orig_video_isVGA = 0x70; /* EFI framebuffer */

		si->lfb_base = lfb_base;
		si->lfb_size = lfb_size;
		si->lfb_width = lfb_width;
		si->lfb_height = lfb_height;
		si->pages = 1;

		switch (pixel_fmt) {
		case PixelRedGreenBlueReserved8BitPerColor:
			si->lfb_depth = 32;
			si->lfb_linelength = pixel_scanline * 4;
			si->red_size = 8;
			si->red_pos = 0;
			si->green_size = 8;
			si->green_pos = 8;
			si->blue_size = 8;
			si->blue_pos = 16;
			si->rsvd_size = 8;
			si->rsvd_pos = 24;
			break;
		case PixelBlueGreenRedReserved8BitPerColor:
			si->lfb_depth = 32;
			si->lfb_linelength = pixel_scanline * 4;
			si->red_size = 8;
			si->red_pos = 16;
			si->green_size = 8;
			si->green_pos = 8;
			si->blue_size = 8;
			si->blue_pos = 0;
			si->rsvd_size = 8;
			si->rsvd_pos = 24;
			break;
		case PixelBitMask:
			bit_mask(pixel_info.RedMask, &si->red_pos,
				 &si->red_size);
			bit_mask(pixel_info.GreenMask, &si->green_pos,
				 &si->green_size);
			bit_mask(pixel_info.BlueMask, &si->blue_pos,
				 &si->blue_size);
			bit_mask(pixel_info.ReservedMask, &si->rsvd_pos,
				 &si->rsvd_size);
			si->lfb_depth = si->red_size + si->green_size +
				si->blue_size + si->rsvd_size;
			si->lfb_linelength = (pixel_scanline * si->lfb_depth) / 8;
			break;
		default:
			si->lfb_depth = 4;;
			si->lfb_linelength = si->lfb_width / 2;
			si->red_size = 0;
			si->red_pos = 0;
			si->green_size = 0;
			si->green_pos = 0;
			si->blue_size = 0;
			si->blue_pos = 0;
			si->rsvd_size = 0;
			si->rsvd_pos = 0;
			break;
		}
	}

out:
	FreePool(handles);
}
