/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * linux.c
 *
 * Sample module to load Linux kernels.  This module can also create
 * a file out of the DHCP return data if running under PXELINUX.
 *
 * If -dhcpinfo is specified, the DHCP info is written into the file
 * /dhcpinfo.dat in the initramfs.
 *
 * Usage: linux.c32 [-dhcpinfo] kernel arguments...
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <syslinux/loadfile.h>
#include <syslinux/linux.h>
#include <syslinux/pxe.h>

const char *progname = "linux.c32";

/* Find the last instance of a particular command line argument
   (which should include the final =; do not use for boolean arguments) */
static char *find_argument(char **argv, const char *argument)
{
    int la = strlen(argument);
    char **arg;
    char *ptr = NULL;

    for (arg = argv; *arg; arg++) {
	if (!memcmp(*arg, argument, la))
	    ptr = *arg + la;
    }

    return ptr;
}

/* Search for a boolean argument; return its position, or 0 if not present */
static int find_boolean(char **argv, const char *argument)
{
    char **arg;

    for (arg = argv; *arg; arg++) {
	if (!strcmp(*arg, argument))
	    return (arg - argv) + 1;
    }

    return 0;
}

/* Stitch together the command line from a set of argv's */
static char *make_cmdline(char **argv)
{
    char **arg;
    size_t bytes;
    char *cmdline, *p;

    bytes = 1;			/* Just in case we have a zero-entry cmdline */
    for (arg = argv; *arg; arg++) {
	bytes += strlen(*arg) + 1;
    }

    p = cmdline = malloc(bytes);
    if (!cmdline)
	return NULL;

    for (arg = argv; *arg; arg++) {
	int len = strlen(*arg);
	memcpy(p, *arg, len);
	p[len] = ' ';
	p += len + 1;
    }

    if (p > cmdline)
	p--;			/* Remove the last space */
    *p = '\0';

    return cmdline;
}

int main(int argc, char *argv[])
{
    const char *kernel_name;
    struct initramfs *initramfs;
    char *cmdline;
    char *boot_image;
    void *kernel_data;
    size_t kernel_len;
    bool opt_dhcpinfo = false;
    bool opt_quiet = false;
    void *dhcpdata;
    size_t dhcplen;
    char **argp, *arg, *p;

    openconsole(&dev_null_r, &dev_stdcon_w);

    (void)argc;
    argp = argv + 1;

    while ((arg = *argp) && arg[0] == '-') {
	if (!strcmp("-dhcpinfo", arg)) {
	    opt_dhcpinfo = true;
	} else {
	    fprintf(stderr, "%s: unknown option: %s\n", progname, arg);
	    return 1;
	}
	argp++;
    }

    if (!arg) {
	fprintf(stderr, "%s: missing kernel name\n", progname);
	return 1;
    }

    kernel_name = arg;

    boot_image = malloc(strlen(kernel_name) + 12);
    if (!boot_image)
	goto bail;
    strcpy(boot_image, "BOOT_IMAGE=");
    strcpy(boot_image + 11, kernel_name);
    /* argp now points to the kernel name, and the command line follows.
       Overwrite the kernel name with the BOOT_IMAGE= argument, and thus
       we have the final argument. */
    *argp = boot_image;

    if (find_boolean(argp, "quiet"))
	opt_quiet = true;

    if (!opt_quiet)
	printf("Loading %s... ", kernel_name);
    if (loadfile(kernel_name, &kernel_data, &kernel_len)) {
	if (opt_quiet)
	    printf("Loading %s ", kernel_name);
	printf("failed!\n");
	goto bail;
    }
    if (!opt_quiet)
	printf("ok\n");

    cmdline = make_cmdline(argp);
    if (!cmdline)
	goto bail;

    /* Initialize the initramfs chain */
    initramfs = initramfs_init();
    if (!initramfs)
	goto bail;

    if ((arg = find_argument(argp, "initrd="))) {
	do {
	    p = strchr(arg, ',');
	    if (p)
		*p = '\0';

	    if (!opt_quiet)
		printf("Loading %s... ", arg);
	    if (initramfs_load_archive(initramfs, arg)) {
		if (opt_quiet)
		    printf("Loading %s ", kernel_name);
		printf("failed!\n");
		goto bail;
	    }
	    if (!opt_quiet)
		printf("ok\n");

	    if (p)
		*p++ = ',';
	} while ((arg = p));
    }

    /* Append the DHCP info */
    if (opt_dhcpinfo &&
	!pxe_get_cached_info(PXENV_PACKET_TYPE_DHCP_ACK, &dhcpdata, &dhcplen)) {
	if (initramfs_add_file(initramfs, dhcpdata, dhcplen, dhcplen,
			       "/dhcpinfo.dat", 0, 0755))
	    goto bail;
    }

    /* This should not return... */
    syslinux_boot_linux(kernel_data, kernel_len, initramfs, cmdline);

bail:
    fprintf(stderr, "Kernel load failure (insufficient memory?)\n");
    return 1;
}
