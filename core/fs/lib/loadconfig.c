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
    realpath(ConfigName, "extlinux.conf", FILENAME_MAX);

    printf("config = %s\n", ConfigName);

    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
    call16(core_open, &regs, &regs);

    return (regs.eflags.l & EFLAGS_ZF) ? -1 : 0;
}
