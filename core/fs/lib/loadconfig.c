#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>

/*
 * Standard version of load_config for extlinux/syslinux filesystems.
 *
 * This searches for extlinux.conf and syslinux.cfg in the install
 * directory, followed by a set of fallback directories.  If found,
 * set the current working directory to match.
 */
int generic_load_config(void)
{
    static const char *search_directories[] = {
	NULL,			/* CurrentDirName */
	"/boot/syslinux", 
	"/syslinux",
	"/",
	NULL
    };
    static const char *filenames[] = {
	"extlinux.conf",
	"syslinux.cfg",
	NULL
    };
    com32sys_t regs;
    int i, j;

    search_directories[0] = CurrentDirName;

    dprintf("CurrentDirName: \"%s\"\n", CurrentDirName);

    for (i = *CurrentDirName ? 0 : 1; search_directories[i]; i++) {
	const char *sd = search_directories[i];
	for (j = 0; filenames[j]; j++) {
	    memset(&regs, 0, sizeof regs);
	    snprintf(ConfigName, FILENAME_MAX, "%s%s%s",
		     sd, (*sd && sd[strlen(sd)-1] == '/') ? "" : "/",
		     filenames[j]);
	    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
	    dprintf("Config search: %s\n", ConfigName);
	    call16(core_open, &regs, &regs);
	    if (!(regs.eflags.l & EFLAGS_ZF)) {
		chdir(search_directories[i]);
		return 0;	/* Got it */
	    }
	}
    }

    return -1;
}
