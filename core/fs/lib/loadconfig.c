#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>

int search_config(const char *search_directories[], const char *filenames[])
{
    char confignamebuf[FILENAME_MAX];
    com32sys_t regs;
    const char *sd, **sdp;
    const char *sf, **sfp;

    for (sdp = search_directories; (sd = *sdp); sdp++) {
	for (sfp = filenames; (sf = *sfp); sfp++) {
	    memset(&regs, 0, sizeof regs);
	    snprintf(confignamebuf, sizeof confignamebuf,
		     "%s%s%s",
		     sd, (*sd && sd[strlen(sd)-1] == '/') ? "" : "/",
		     sf);
	    realpath(ConfigName, confignamebuf, FILENAME_MAX);
	    regs.edi.w[0] = OFFS_WRT(ConfigName, 0);
	    dprintf("Config search: %s\n", ConfigName);
	    call16(core_open, &regs, &regs);
	    if (!(regs.eflags.l & EFLAGS_ZF)) {
		chdir(sd);
		return 0;	/* Got it */
	    }
	}
    }

    return -1;
}

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

    search_directories[0] = CurrentDirName;

    dprintf("CurrentDirName: \"%s\"\n", CurrentDirName);

    return search_config(search_directories, filenames);
}
