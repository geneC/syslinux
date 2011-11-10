#include <sys/io.h>
#include <fs.h>
#include <syslinux/memscan.h>
#include <bios.h>
#include <syslinux/firmware.h>

struct firmware *firmware = NULL;

struct firmware bios_fw = {
	.init = bios_init,
	.scan_memory = bios_scan_memory,
	.adjust_screen = bios_adjust_screen,
	.cleanup = bios_cleanup_hardware,
};

void syslinux_register_bios(void)
{
	firmware = &bios_fw;
}
