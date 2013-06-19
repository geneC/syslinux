/*
 * We don't have separate boot loader derivatives under EFI, rather,
 * the derivative info reflects the capabilities of the machine. For
 * instance, if we have the PXE Base Code Protocol, then we support
 * PXELINUX, if we have the Disk I/O Protocol, we support SYSLINUX,
 * etc.
 */
#include <syslinux/config.h>

/*
 * IP information.  Note that the field are in the same order as the
 * Linux kernel expects in the ip= option.
 */
struct syslinux_ipinfo IPInfo;
uint16_t APIVer;		/* PXE API version found */

__export void get_derivative_info(union syslinux_derivative_info *di)
{
	di->disk.filesystem = SYSLINUX_FS_SYSLINUX;
}
