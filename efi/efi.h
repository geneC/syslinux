#ifndef _SYSLINUX_EFI_H
#define _SYSLINUX_EFI_H

#include <core.h>
#include <sys/types.h>	/* needed for off_t */
//#include <syslinux/version.h> /* avoid redefinition of __STDC_VERSION__ */
#include <efi.h>
#include <efilib.h>
#include <efistdarg.h>

struct efi_disk_private {
	EFI_HANDLE dev_handle;
	EFI_BLOCK_IO *bio;
	EFI_DISK_IO *dio;
};

extern EFI_HANDLE image_handle;

struct screen_info;
extern void setup_screen(struct screen_info *);

extern void efi_write_char(uint8_t, uint8_t);

enum heap;
extern void *efi_malloc(size_t, enum heap, size_t);
extern void *efi_realloc(void *, size_t);
extern void efi_free(void *);

#endif /* _SYSLINUX_EFI_H */
