#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chain.h"
#include "utility.h"
#include "options.h"

int soi_s2n(char *ptr, unsigned int *seg,
		       unsigned int *off,
		       unsigned int *ip)
{
    unsigned int segval = 0, offval = 0, ipval = 0, val;
    char *p;

    segval = strtoul(ptr, &p, 0);
    if (*p == ':')
	offval = strtoul(p+1, &p, 0);
    if (*p == ':')
	ipval = strtoul(p+1, NULL, 0);

    val = (segval << 4) + offval;

    if (val < ADDRMIN || val > ADDRMAX) {
	error("Invalid seg:off:* address specified..\n");
	goto bail;
    }

    val = (segval << 4) + ipval;

    if (ipval > 0xFFFE || val < ADDRMIN || val > ADDRMAX) {
	error("Invalid seg:*:ip address specified.\n");
	goto bail;
    }

    if (seg)
	*seg = segval;
    if (off)
	*off = offval;
    if (ip)
	*ip  = ipval;

    return 0;
bail:
    return -1;
}

void usage(void)
{
    static const char *const usage[] = { "\
Usage:\n\
    chain.c32 [options]\n\
    chain.c32 {fd|hd}<disk> [<partition>] [options]\n\
    chain.c32 mbr{:|=}<id> [<partition>] [options]\n\
    chain.c32 guid{:|=}<guid> [<partition>] [options]\n\
    chain.c32 label{:|=}<label> [<partition>] [options]\n\
    chain.c32 boot{,| }[<partition>] [options]\n\
    chain.c32 fs [options]\n\
\nOptions ('no' prefix specify defaulti value):\n\
    file=<loader>        Load and execute file\n\
    seg=<s[:o[:i]]>      Load file at <s:o>, jump to <s:i>\n\
    nofilebpb            Treat file in memory as BPB compatible\n\
    sect[=<s[:o[:i]]>]   Load sector at <s:o>, jump to <s:i>\n\
                         - defaults to 0:0x7C00:0x7C00\n\
    maps                 Map loaded sector into real memory\n\
    nosethid[den]        Set BPB's hidden sectors field\n\
    nosetgeo             Set BPB's sectors per track and heads fields\n\
    nosetdrv[@<off>]     Set BPB's drive unit field at <o>\n\
                         - <off> defaults to autodetection\n\
                         - only 0x24 and 0x40 are accepted\n\
    nosetbpb             Enable set{hid,geo,drv}\n\
    nosave               Write adjusted sector back to disk\n\
    hand                 Prepare handover area\n\
    nohptr               Force ds:si and ds:bp to point to handover area\n\
    noswap               Swap drive numbers, if bootdisk is not fd0/hd0\n\
    nohide               Hide primary partitions, unhide selected partition\n\
    nokeeppxe            Keep the PXE and UNDI stacks in memory (PXELINUX)\n\
    nowarn               Wait for a keypress to continue chainloading\n\
                         - useful to see emited warnings\n\
", "\
\nComposite options:\n\
    isolinux=<loader>    Load another version of ISOLINUX\n\
    ntldr=<loader>       Load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n\
    cmldr=<loader>       Load Recovery Console of Windows NT/2K/XP/2003\n\
    freedos=<loader>     Load FreeDOS KERNEL.SYS\n\
    msdos=<loader>       Load MS-DOS 2.xx - 6.xx IO.SYS\n\
    msdos7=<loader>      Load MS-DOS 7+ IO.SYS\n\
    pcdos=<loader>       Load PC-DOS IBMBIO.COM\n\
    drmk=<loader>        Load DRMK DELLBIO.BIN\n\
    grub=<loader>        Load GRUB Legacy stage2\n\
    grubcfg=<filename>   Set alternative config filename for GRUB Legacy\n\
    grldr=<loader>       Load GRUB4DOS grldr\n\
\nPlease see doc/chain.txt for the detailed documentation.\n"
    };
    error(usage[0]);
    error("Press any key...\n");
    wait_key();
    error(usage[1]);
}

int parse_args(int argc, char *argv[])
{
    int i;
    unsigned int v;
    char *p;

    for (i = 1; i < argc; i++) {
	if (!strncmp(argv[i], "file=", 5)) {
	    opt.file = argv[i] + 5;
	} else if (!strcmp(argv[i], "nofile")) {
	    opt.file = NULL;
	} else if (!strncmp(argv[i], "seg=", 4)) {
	    if (soi_s2n(argv[i] + 4, &opt.fseg, &opt.foff, &opt.fip))
		goto bail;
	} else if (!strncmp(argv[i], "isolinux=", 9)) {
	    opt.file = argv[i] + 9;
	    opt.isolinux = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strncmp(argv[i], "ntldr=", 6)) {
	    opt.fseg = 0x2000;  /* NTLDR wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.file = argv[i] + 6;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = 0x24;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.fseg = 0x2000;  /* CMLDR wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.file = argv[i] + 6;
	    opt.cmldr = true;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = 0x24;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "freedos=", 8)) {
	    opt.fseg = 0x60;    /* FREEDOS wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.sseg = 0x9000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + 8;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if ( (v = 6, !strncmp(argv[i], "msdos=", v) ||
		     !strncmp(argv[i], "pcdos=", v)) ||
		    (v = 7, !strncmp(argv[i], "msdos7=", v)) ) {
	    opt.fseg = 0x70;    /* MS-DOS 2.00 .. 6.xx wants this address */
	    opt.foff = 0;
	    opt.fip = v == 7 ? 0x200 : 0;  /* MS-DOS 7.0+ wants this ip */
	    opt.sseg = 0x9000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + v;
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "drmk=", 5)) {
	    opt.fseg = 0x70;    /* DRMK wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.sseg = 0x2000;
	    opt.soff = 0;
	    opt.sip = 0;
	    opt.file = argv[i] + 5;
	    /* opt.drmk = true; */
	    opt.sethid = true;
	    opt.setgeo = true;
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "grub=", 5)) {
	    opt.fseg = 0x800;	/* stage2 wants this address */
	    opt.foff = 0;
	    opt.fip = 0x200;
	    opt.file = argv[i] + 5;
	    opt.grub = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strncmp(argv[i], "grubcfg=", 8)) {
	    opt.grubcfg = argv[i] + 8;
	} else if (!strncmp(argv[i], "grldr=", 6)) {
	    opt.file = argv[i] + 6;
	    opt.grldr = true;
	    opt.hand = false;
	    opt.sect = false;
	} else if (!strcmp(argv[i], "keeppxe")) {
	    opt.keeppxe = 3;
	} else if (!strcmp(argv[i], "nokeeppxe")) {
	    opt.keeppxe = 0;
	} else if (!strcmp(argv[i], "maps")) {
	    opt.maps = true;
	} else if (!strcmp(argv[i], "nomaps")) {
	    opt.maps = false;
	} else if (!strcmp(argv[i], "hand")) {
	    opt.hand = true;
	} else if (!strcmp(argv[i], "nohand")) {
	    opt.hand = false;
	} else if (!strcmp(argv[i], "hptr")) {
	    opt.hptr = true;
	} else if (!strcmp(argv[i], "nohptr")) {
	    opt.hptr = false;
	} else if (!strcmp(argv[i], "swap")) {
	    opt.swap = true;
	} else if (!strcmp(argv[i], "noswap")) {
	    opt.swap = false;
	} else if (!strcmp(argv[i], "hide")) {
	    opt.hide = true;
	} else if (!strcmp(argv[i], "nohide")) {
	    opt.hide = false;
	} else if (!strcmp(argv[i], "sethid") ||
		   !strcmp(argv[i], "sethidden")) {
	    opt.sethid = true;
	} else if (!strcmp(argv[i], "nosethid") ||
		   !strcmp(argv[i], "nosethidden")) {
	    opt.sethid = false;
	} else if (!strcmp(argv[i], "setgeo")) {
	    opt.setgeo = true;
	} else if (!strcmp(argv[i], "nosetgeo")) {
	    opt.setgeo = false;
	} else if (!strncmp(argv[i], "setdrv",6)) {
	    if (!argv[i][6])
		v = ~0u;    /* autodetect */
	    else if (argv[i][6] == '@' ||
		    argv[i][6] == '=' ||
		    argv[i][6] == ':') {
		v = strtoul(argv[i] + 7, NULL, 0);
		if (!(v == 0x24 || v == 0x40)) {
		    error("Invalid 'setdrv' offset.\n");
		    goto bail;
		}
	    } else {
		    error("Invalid 'setdrv' specification.\n");
		    goto bail;
		}
	    opt.setdrv = true;
	    opt.drvoff = v;
	} else if (!strcmp(argv[i], "nosetdrv")) {
	    opt.setdrv = false;
	} else if (!strcmp(argv[i], "setbpb")) {
	    opt.setdrv = true;
	    opt.drvoff = ~0u;
	    opt.setgeo = true;
	    opt.sethid = true;
	} else if (!strcmp(argv[i], "nosetbpb")) {
	    opt.setdrv = false;
	    opt.setgeo = false;
	    opt.sethid = false;
	} else if (!strncmp(argv[i], "sect=", 5) ||
		   !strcmp(argv[i], "sect")) {
	    if (argv[i][4]) {
		if (soi_s2n(argv[i] + 5, &opt.sseg, &opt.soff, &opt.sip))
		    goto bail;
		if ((opt.sseg << 4) + opt.soff + SECTOR - 1 > ADDRMAX) {
		    error("Arguments of 'sect=' are invalid - resulting address too big.\n");
		    goto bail;
		}
	    }
	    opt.sect = true;
	} else if (!strcmp(argv[i], "nosect")) {
	    opt.sect = false;
	} else if (!strcmp(argv[i], "save")) {
	    opt.save = true;
	} else if (!strcmp(argv[i], "nosave")) {
	    opt.save = false;
	} else if (!strcmp(argv[i], "filebpb")) {
	    opt.filebpb = true;
	} else if (!strcmp(argv[i], "nofilebpb")) {
	    opt.filebpb = false;
	} else if (!strcmp(argv[i], "warn")) {
	    opt.warn = true;
	} else if (!strcmp(argv[i], "nowarn")) {
	    opt.warn = false;
	} else if (((argv[i][0] == 'h' || argv[i][0] == 'f')
		    && argv[i][1] == 'd')
		   || !strncmp(argv[i], "mbr:", 4)
		   || !strncmp(argv[i], "mbr=", 4)
		   || !strncmp(argv[i], "guid:", 5)
		   || !strncmp(argv[i], "guid=", 5)
		   || !strncmp(argv[i], "label:", 6)
		   || !strncmp(argv[i], "label=", 6)
		   || !strcmp(argv[i], "boot")
		   || !strncmp(argv[i], "boot,", 5)
		   || !strcmp(argv[i], "fs")) {
	    opt.drivename = argv[i];
	    p = strchr(opt.drivename, ',');
	    if (p) {
		*p = '\0';
		opt.partition = p + 1;
	    } else if (argv[i + 1] && argv[i + 1][0] >= '0'
		       && argv[i + 1][0] <= '9') {
		opt.partition = argv[++i];
	    }
	} else {
	    usage();
	    goto bail;
	}
    }

    if (opt.grubcfg && !opt.grub) {
	error("grubcfg=<filename> must be used together with grub=<loader>.\n");
	goto bail;
    }

    if ((!opt.maps || !opt.sect) && !opt.file) {
	error("You have to load something.\n");
	goto bail;
    }

    if (opt.filebpb && !opt.file) {
	error("Option 'filebpb' requires file.\n");
	goto bail;
    }

    return 0;
bail:
    return -1;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
