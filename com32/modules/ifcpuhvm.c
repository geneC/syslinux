/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * ifcpuhvm.c
 *
 * Run one command if the CPU has hardware virtualisation support,
 * and another if it doesn't.
 * Eventually this and other features should get folded into some kind
 * of scripting engine.
 *
 * Usage:
 *
 *    label boot_kernel
 *        kernel ifvhm.c32
 *        append boot_kernel_xen -- boot_kernel_regular
 *    label boot_kernel_xen
 *    	  kernel mboot.c32
 *        append xen.gz dom0_mem=262144 -- vmlinuz-xen console=tty0 root=/dev/hda1 ro --- initrd.img-xen
 *    label boot_kernel_regular
 *        kernel vmlinuz_64
 *        append ...
 */

#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <cpuid.h>
#include <syslinux/boot.h>

/* XXX: this really should be librarized */
static void boot_args(char **args)
{
    int len = 0;
    char **pp;
    const char *p;
    char c, *q, *str;

    for (pp = args; *pp; pp++)
	len += strlen(*pp);

    q = str = alloca(len + 1);
    for (pp = args; *pp; pp++) {
	p = *pp;
	while ((c = *p++))
	    *q++ = c;
    }
    *q = '\0';

    if (!str[0])
	syslinux_run_default();
    else
	syslinux_run_command(str);
}

int main(int argc, char *argv[])
{
    char **args[3];
    int i;
    int n;
    s_cpu cpu;
    detect_cpu(&cpu);

    args[0] = &argv[1];
    n = 1;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "--")) {
	    argv[i] = NULL;
	    args[n++] = &argv[i + 1];
	}
	if (n >= 3)
	    break;
    }
    while (n < 3) {
	args[n] = args[n - 1];
	n++;
    }

    boot_args((cpu.flags.vmx || cpu.flags.svm) ? args[0] : args[1]);
    return -1;
}
