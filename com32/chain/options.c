#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chain.h"
#include "utility.h"
#include "options.h"

struct options opt;

static int soi_s2n(char *ptr, unsigned int *seg,
		       unsigned int *off,
		       unsigned int *ip,
		       unsigned int def)
{
    unsigned int segval = 0, offval, ipval, val;
    char *p;

    offval = def;
    ipval = def;

    segval = strtoul(ptr, &p, 0);
    if (p[0] == ':' && p[1] && p[1] != ':')
	offval = strtoul(p+1, &p, 0);
    if (p[0] == ':' && p[1] && p[1] != ':')
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

static void usage(void)
{
    unsigned int i;
    static const char key[] = "Press any key...\n";
    static const char *const usage[] = {
"\
Usage:\n\
    chain.c32 [options]\n\
    chain.c32 {fd|hd}<disk#>{,| }[<part#>] [options]\n\
    chain.c32 mbr{:|=}<id>{,| }[<part#>] [options]\n\
    chain.c32 guid{:|=}<guid>{,| }[<part#>] [options]\n\
    chain.c32 label{:|=}<label> [<part#>] [options]\n\
    chain.c32 boot{,| }[<part#>] [options]\n\
    chain.c32 fs [options]\n\
", "\
\nOptions ('no' prefix specifies default value):\n\
    sect[=<s[:o[:i]]>]   Load sector at <s:o>, jump to <s:i>\n\
                         - defaults to 0:0x7C00:0x7C00\n\
                         - ommited o/i values default to 0\n\
    maps                 Map loaded sector into real memory\n\
    nosetbpb             Fix BPB fields in loaded sector\n\
    nofilebpb            Apply 'setbpb' to loaded file\n\
    nosave               Write adjusted sector back to disk\n\
    hand                 Prepare handover area\n\
    nohptr               Force ds:si and ds:bp to point to handover area\n\
    noswap               Swap drive numbers, if bootdisk is not fd0/hd0\n\
    nohide               Disable all hide variations (also the default)\n\
    hide                 Hide primary partitions, unhide selected partition\n\
    hideall              Hide *all* partitions, unhide selected partition\n\
    unhide               Unhide primary partitions\n\
    unhideall            Unhide *all* partitions\n\
    nofixchs             Walk *all* partitions and fix E/MBRs' chs values\n\
    nokeeppxe            Keep the PXE and UNDI stacks in memory (PXELINUX)\n\
    nowarn               Wait for a keypress to continue chainloading\n\
                         - useful to see emited warnings\n\
    nobreak              Actually perform the chainloading\n\
", "\
\nOptions continued ...\n\
    file=<file>          Load and execute <file>\n\
    seg=<s[:o[:i]]>      Load file at <s:o>, jump to <s:i>\n\
                         - defaults to 0:0x7C00:0x7C00\n\
                         - ommited o/i values default to 0\n\
    isolinux=<loader>    Load another version of ISOLINUX\n\
    ntldr=<loader>       Load Windows NTLDR, SETUPLDR.BIN or BOOTMGR\n\
    reactos=<loader>     Load ReactOS's loader\n\
    cmldr=<loader>       Load Recovery Console of Windows NT/2K/XP/2003\n\
    freedos=<loader>     Load FreeDOS KERNEL.SYS\n\
    msdos=<loader>       Load MS-DOS 2.xx - 6.xx IO.SYS\n\
    msdos7=<loader>      Load MS-DOS 7+ IO.SYS\n\
    pcdos=<loader>       Load PC-DOS IBMBIO.COM\n\
    drmk=<loader>        Load DRMK DELLBIO.BIN\n\
    grub=<loader>        Load GRUB Legacy stage2\n\
    grubcfg=<filename>   Set alternative config filename for GRUB Legacy\n\
    grldr=<loader>       Load GRUB4DOS grldr\n\
    bss=<filename>       Emulate syslinux's BSS\n\
    bs=<filename>        Emulate syslinux's BS\n\
\nPlease see doc/chain.txt for the detailed documentation.\n\
"
    };
    for (i = 0; i < sizeof(usage)/sizeof(usage[0]); i++) {
	if (i) {
	    error(key);
	    wait_key();
	}
	error(usage[i]);
    }
}

void opt_set_defs(void)
{
    memset(&opt, 0, sizeof(opt));
    opt.sect = true;	    /* by def. load sector */
    opt.maps = true;	    /* by def. map sector */
    opt.hand = true;	    /* by def. prepare handover */
    opt.brkchain = false;   /* by def. do chainload */
    opt.foff = opt.soff = opt.fip = opt.sip = 0x7C00;
    opt.drivename = "boot";
#ifdef DEBUG
    opt.warn = true;
#endif
}

int opt_parse_args(int argc, char *argv[])
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
	    if (soi_s2n(argv[i] + 4, &opt.fseg, &opt.foff, &opt.fip, 0))
		goto bail;
	} else if (!strncmp(argv[i], "bss=", 4)) {
	    opt.file = argv[i] + 4;
	    opt.bss = true;
	    opt.maps = false;
	    opt.setbpb = true;
	    /* opt.save = true; */
	} else if (!strncmp(argv[i], "bs=", 3)) {
	    opt.file = argv[i] + 3;
	    opt.sect = false;
	    opt.filebpb = true;
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
	    opt.setbpb = true;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "reactos=", 8)) {
	    /*
	     * settings based on commit
	     *   ad4cf1470977f648ee1dd45e97939589ccb0393c
	     * note, conflicts with:
	     *   http://reactos.freedoors.org/Reactos%200.3.13/ReactOS-0.3.13-REL-src/boot/freeldr/notes.txt
	     */
	    opt.fseg = 0;
	    opt.foff = 0x8000;
	    opt.fip = 0x8100;
	    opt.file = argv[i] + 8;
	    opt.setbpb = true;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "cmldr=", 6)) {
	    opt.fseg = 0x2000;  /* CMLDR wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.file = argv[i] + 6;
	    opt.cmldr = true;
	    opt.setbpb = true;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if (!strncmp(argv[i], "freedos=", 8)) {
	    opt.fseg = 0x60;    /* FREEDOS wants this address */
	    opt.foff = 0;
	    opt.fip = 0;
	    opt.sseg = 0x1FE0;
	    opt.file = argv[i] + 8;
	    opt.setbpb = true;
	    /* opt.save = true; */
	    opt.hand = false;
	} else if ( (v = 6, !strncmp(argv[i], "msdos=", v) ||
		     !strncmp(argv[i], "pcdos=", v)) ||
		    (v = 7, !strncmp(argv[i], "msdos7=", v)) ) {
	    opt.fseg = 0x70;    /* MS-DOS 2.00 .. 6.xx wants this address */
	    opt.foff = 0;
	    opt.fip = v == 7 ? 0x200 : 0;  /* MS-DOS 7.0+ wants this ip */
	    opt.sseg = 0x8000;
	    opt.file = argv[i] + v;
	    opt.setbpb = true;
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
	    opt.setbpb = true;
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
	} else if (!strcmp(argv[i], "nohide")) {
	    opt.hide = 0;
	} else if (!strcmp(argv[i], "hide")) {
	    opt.hide = 1; /* 001b */
	} else if (!strcmp(argv[i], "hideall")) {
	    opt.hide = 2; /* 010b */
	} else if (!strcmp(argv[i], "unhide")) {
	    opt.hide = 5; /* 101b */
	} else if (!strcmp(argv[i], "unhideall")) {
	    opt.hide = 6; /* 110b */
	} else if (!strcmp(argv[i], "setbpb")) {
	    opt.setbpb = true;
	} else if (!strcmp(argv[i], "nosetbpb")) {
	    opt.setbpb = false;
	} else if (!strcmp(argv[i], "filebpb")) {
	    opt.filebpb = true;
	} else if (!strcmp(argv[i], "nofilebpb")) {
	    opt.filebpb = false;
	} else if (!strncmp(argv[i], "sect=", 5) ||
		   !strcmp(argv[i], "sect")) {
	    if (argv[i][4]) {
		if (soi_s2n(argv[i] + 5, &opt.sseg, &opt.soff, &opt.sip, 0))
		    goto bail;
	    }
	    opt.sect = true;
	} else if (!strcmp(argv[i], "nosect")) {
	    opt.sect = false;
	    opt.maps = false;
	} else if (!strcmp(argv[i], "save")) {
	    opt.save = true;
	} else if (!strcmp(argv[i], "nosave")) {
	    opt.save = false;
	} else if (!strcmp(argv[i], "fixchs")) {
	    opt.fixchs = true;
	} else if (!strcmp(argv[i], "nofixchs")) {
	    opt.fixchs = false;
	} else if (!strcmp(argv[i], "warn")) {
	    opt.warn = true;
	} else if (!strcmp(argv[i], "nowarn")) {
	    opt.warn = false;
	} else if (!strcmp(argv[i], "nobreak")) {
	    opt.brkchain = false;
	} else if (!strcmp(argv[i], "break")) {
	    opt.brkchain = true;
	    opt.file = NULL;
	    opt.maps = false;
	    opt.hand = false;
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
	    if (strncmp(argv[i], "label", 5))
		p = strchr(opt.drivename, ',');
	    else
		p = NULL;
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

#if 0
    if ((!opt.maps || !opt.sect) && !opt.file) {
	error("You have to load something.\n");
	goto bail;
    }
#endif

    if (opt.filebpb && !opt.file) {
	error("Option 'filebpb' requires a file.\n");
	goto bail;
    }

    if (opt.save && !opt.sect) {
	error("Option 'save' requires a sector.\n");
	goto bail;
    }

    if (opt.setbpb && !opt.sect) {
	error("Option 'setbpb' requires a sector.\n");
	goto bail;
    }

    if (opt.maps && !opt.sect) {
	error("Option 'maps' requires a sector.\n");
	goto bail;
    }

    return 0;
bail:
    return -1;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
