#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>

/*
 * Standard version of load_config for extlinux/syslinux filesystems
 */
int generic_load_config(void)
{
    com32sys_t regs;

    chdir(CurrentDirName);
    /* try extlinux.conf first */
    realpath(ConfigName, "extlinux.conf", FILENAME_MAX);
    dprintf("Try config = %s\n", ConfigName);
    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
    call16(core_open, &regs, &regs);
    /* give syslinux.cfg a chance ? */
    if (regs.eflags.l & EFLAGS_ZF) {
	realpath(ConfigName, "syslinux.cfg", FILENAME_MAX);
	dprintf("Then try config = %s\n", ConfigName);
	memset(&regs, 0, sizeof regs);
	regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
	call16(core_open, &regs, &regs);
    }
    return (regs.eflags.l & EFLAGS_ZF) ? -1 : 0;
}
