/*
 * EFI image boot capabilities by Patrick Masotta (Serva) (c)2015
 */

/*
 * load_efi.c
 *
 * Load an efi image
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <minmax.h>
#include <errno.h>
#include <suffix_number.h>
#include <dprintf.h>

#include <syslinux/align.h>
#include <syslinux/linux.h>
#include <syslinux/bootrm.h>
#include <syslinux/movebits.h>
#include <syslinux/firmware.h>
#include <syslinux/video.h>



int syslinux_boot_efi(void *kernel_buf, size_t kernel_size,
			char *cmdline, int cmdlineSize)
{
    if (firmware->boot_efi)
	return firmware->boot_efi(kernel_buf, kernel_size, cmdline, cmdlineSize);

    return -1;
}
