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
int search_config(struct com32_filedata *filedata,
		  const char *search_directories[], const char *filenames[])
{
    char confignamebuf[FILENAME_MAX];
    const char *sd, **sdp;
    const char *sf, **sfp;

    for (sdp = search_directories; (sd = *sdp); sdp++) {
	for (sfp = filenames; (sf = *sfp); sfp++) {
	    snprintf(confignamebuf, sizeof confignamebuf,
		     "%s%s%s",
		     sd, (*sd && sd[strlen(sd)-1] == '/') ? "" : "/",
		     sf);
	    realpath(ConfigName, confignamebuf, FILENAME_MAX);
	    dprintf("Config search: %s\n", ConfigName);
	    if (open_file(ConfigName, filedata) >= 0) {
		chdir(sd);
		return 0;	/* Got it */
	    }
	}
    }

    return -1;
}
