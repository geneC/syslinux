#ifndef _SYSLINUX_EFI_H
#define _SYSLINUX_EFI_H

#include <core.h>
#include <sys/types.h>	/* needed for off_t */
//#include <syslinux/version.h> /* avoid redefinition of __STDC_VERSION__ */
#include <efi.h>
#include <efilib.h>
#include <efistdarg.h>

extern EFI_HANDLE image_handle;
void setup_screen(struct screen_info *); /* fix build error */

#endif /* _SYSLINUX_EFI_H */
