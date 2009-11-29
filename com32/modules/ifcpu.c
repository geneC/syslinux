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
 * ifcpu.c
 *
 * Run one command (boot_entry_1) if the booted system match some CPU features
 * and another (boot_entry_2) if it doesn't.
 * Eventually this and other features should get folded into some kind
 * of scripting engine.
 *
 * Usage:
 *
 *    label test
 *        com32 ifcpu.c32
 *        append <option> <cpu_features> -- boot_entry_1 -- boot_entry_2
 *    label boot_entry_1
 *    	  kernel vmlinuz
 *    	  append ...
 *    label boot_entry_2
 *        kernel vmlinuz_64
 *        append ...
 *
 * options could be :
 *    debug     : display some debugging messages
 *    dry-run   : just do the detection, don't boot
 *
 * cpu_features could be:
 *    64        : CPU have to be x86_64 compatible
 *    hvm       : Processor must have hardware virtualization (hvm or svm)
 *    multicore : Processor must be multi-core
 *    smp       : System have to be SMP
 *
 * if you want to match many cpu features, just separate them with a single space
 */

#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cpuid.h>
#include <syslinux/boot.h>
#include <com32.h>
#include <consoles.h>

#define REG_AH(x) ((x).eax.b[1])
#define REG_CX(x) ((x).ecx.w[0])
#define REG_DX(x) ((x).edx.w[0])

static unsigned char sleep(unsigned int msec)
{
    unsigned long micro = 1000 * msec;
    com32sys_t inreg, outreg;

    REG_AH(inreg) = 0x86;
    REG_CX(inreg) = (micro >> 16);
    REG_DX(inreg) = (micro & 0xFFFF);
    __intcall(0x15, &inreg, &outreg);
    return REG_AH(outreg);
}

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

#define show_bool(mybool) mybool ? "found":"not found"

int main(int argc, char *argv[])
{
    char **args[3];
    int i;
    int n;
    bool hardware_matches = true;
    bool multicore = false;
    bool dryrun = false;
    bool debug = false;

    s_cpu cpu;
    console_ansi_raw();
    detect_cpu(&cpu);
    n = 0;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "--")) {
	    argv[i] = NULL;
	    args[n++] = &argv[i + 1];
	} else if (!strcmp(argv[i], "64")) {
	    if (debug)
		printf(" 64bit     : %s on this system\n",
		       show_bool(cpu.flags.lm));
	    hardware_matches = cpu.flags.lm && hardware_matches;
	} else if (!strcmp(argv[i], "hvm")) {
	    if (debug)
		printf(" hvm       : %s on this system\n",
		       show_bool((cpu.flags.vmx || cpu.flags.svm)));
	    hardware_matches = (cpu.flags.vmx || cpu.flags.svm)
		&& hardware_matches;
	} else if (!strcmp(argv[i], "multicore")) {
	    if (debug)
		printf(" multicore : %d cores on this system\n", cpu.num_cores);
	    if (cpu.num_cores > 1)
		multicore = true;
	    hardware_matches = multicore && hardware_matches;
	} else if (!strcmp(argv[i], "smp")) {
	    if (debug)
		printf(" smp       : %s on this system\n", show_bool(cpu.flags.smp));
	    hardware_matches = cpu.flags.smp && hardware_matches;
	} else if (!strcmp(argv[i], "dry-run")) {
	    dryrun = true;
	} else if (!strcmp(argv[i], "debug")) {
	    debug = true;
	}
	if (n >= 2)
	    break;
    }
    while (n < 2) {
	args[n] = args[n - 1];
	n++;
    }
    if (debug) {
	printf("\nBooting labels are : '%s' or '%s'\n", *args[0], *args[1]);
	printf("Hardware requirements%smatch this system, let's booting '%s'\n",
	       hardware_matches ? " " : " doesn't ",
	       hardware_matches ? *args[0] : *args[1]);
	printf("Sleeping 5sec before booting\n");
	if (!dryrun)
	    sleep(5000);
    }

    if (!dryrun)
	boot_args(hardware_matches ? args[0] : args[1]);
    else
	printf("Dry-run mode, let's exiting\n");

    return -1;
}
