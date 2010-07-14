#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>

/*
 * Common implementation of load_config
 *
 * This searches for a specified set of filenames in a specified set
 * of directories.  If found, set the current working directory to
 * match.
 */
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
