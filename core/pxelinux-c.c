#include <syslinux/config.h>
#include <com32.h>

extern far_ptr_t StrucPtr;
extern far_ptr_t InitStack;

/*
 * IP information.  Note that the field are in the same order as the
 * Linux kernel expects in the ip= option.
 */
struct syslinux_ipinfo IPInfo;
uint16_t APIVer;		/* PXE API version found */

void get_derivative_info(union syslinux_derivative_info *di)
{
	di->pxe.filesystem = SYSLINUX_FS_PXELINUX;
	di->pxe.apiver = APIVer;
	di->pxe.pxenvptr = GET_PTR(StrucPtr);
	di->pxe.stack = GET_PTR(InitStack);
	di->pxe.ipinfo = &IPInfo;
	di->pxe.myip = IPInfo.myip;
}
