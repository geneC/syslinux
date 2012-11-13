#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <dprintf.h>
#include <syslinux/loadfile.h>
#include <syslinux/linux.h>
#include <syslinux/pxe.h>
#include "core.h"

const char *globaldefault = NULL;
const char *append = NULL;

/* Will be called from readconfig.c */
int new_linux_kernel(char *okernel, char *ocmdline)
{
	const char *kernel_name = NULL;
	struct initramfs *initramfs = NULL;
	char *temp;
	void *kernel_data;
	size_t kernel_len;
	bool opt_quiet = false;
	char initrd_name[256];
	char cmdline_buf[256], *cmdline;

	dprintf("okernel = %s, ocmdline = %s", okernel, ocmdline);

	cmdline = cmdline_buf;

	temp = cmdline;

	if (okernel)
		kernel_name = okernel;
	else if (globaldefault)
		kernel_name = globaldefault;

	strcpy(temp, kernel_name);
	temp += strlen(kernel_name);

	*temp = ' ';
	temp++;
	if (ocmdline)
		strcpy(temp, ocmdline);
	else if (append)
		strcpy(temp, append);

	/* "keeppxe" handling */
#if IS_PXELINUX
	extern char KeepPXE;

	if (strstr(cmdline, "keeppxe"))
		KeepPXE |= 1;
#endif

	if (strstr(cmdline, "quiet"))
		opt_quiet = true;

	if (!opt_quiet)
		printf("Loading %s... ", kernel_name);

	if (loadfile(kernel_name, &kernel_data, &kernel_len)) {
		if (opt_quiet)
			printf("Loading %s ", kernel_name);
		printf("failed: ");
		goto bail;
	}

	if (!opt_quiet)
		printf("ok\n");

	/* Find and load initramfs */
	temp = strstr(cmdline, "initrd=");
	if (temp) {
		/* Initialize the initramfs chain */
		initramfs = initramfs_init();
		if (!initramfs)
			goto bail;

		temp += 6; /* strlen("initrd") */
		do {
		    char *p = initrd_name;

		    temp++;	/* Skip = or , */

		    while (*temp != ' ' && *temp != ',' && *temp)
			*p++ = *temp++;
		    *p = '\0';

		    if (!opt_quiet)
			printf("Loading %s...", initrd_name);

		    if (initramfs_load_archive(initramfs, initrd_name)) {
			if (opt_quiet)
			    printf("Loading %s ", initrd_name);
			printf("failed: ");
			goto bail;
		    }

		    if (!opt_quiet)
			printf("ok\n");
		} while (*temp == ',');
	}

	/* This should not return... */
	syslinux_boot_linux(kernel_data, kernel_len, initramfs, NULL, cmdline);
	printf("Booting kernel failed: ");

bail:
	printf("%s\n", strerror(errno));
	return 1;
}
