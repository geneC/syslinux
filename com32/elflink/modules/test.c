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
#include <sys/module.h>

//const char *progname = "test.c32";

static int test_main(int argc, char *argv[])
{
    const char *kernel_name;
    struct initramfs *initramfs;
    char *cmdline;
    char *boot_image;
    void *kernel_data;
    size_t kernel_len;
    bool opt_quiet = false;
    char **argp, *arg, *p;

	/*
    while ((arg = *argp) && arg[0] == '-') {
	if (!strcmp("-dhcpinfo", arg)) {
		printf("haha\n");
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
    */

    //kernel_name = arg;
    kernel_name = "vmlinuz";

	printf("Loading %s... ", kernel_name);
    if (loadfile(kernel_name, &kernel_data, &kernel_len)) {
	printf("failed!\n");
	goto bail;
    }
	printf("ok\n");

    /* Initialize the initramfs chain */
    initramfs = initramfs_init();
    if (!initramfs)
	goto bail;


	if (initramfs_load_archive(initramfs, "initrd.gz"))
		mp("initrd load failure\n");
	else
		mp("initrd load success\n");

	mp("1111");

        /* This should not return... */
    syslinux_boot_linux(kernel_data, kernel_len, initramfs, cmdline);

bail:
    fprintf(stderr, "Kernel load failure (insufficient memory?)\n");
    return 1;
}

MODULE_MAIN(test_main);
