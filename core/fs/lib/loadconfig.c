#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>

/*
 * Standard version of load_config for extlinux-installed filesystems
 */
int generic_load_config(void)
{
    com32sys_t regs;

    chdir(CurrentDirName);

    memset(&regs, 0, sizeof regs);
    snprintf(ConfigName, FILENAME_MAX, "%s/extlinux.conf", CurrentDirName);
    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
    call16(core_open, &regs, &regs);

    return (regs.eflags.l & EFLAGS_ZF) ? -1 : 0;
}
