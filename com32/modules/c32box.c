/*
 *
 * c32box.c: contains code from multiple sources: md5.c, ifmem.c, reboot.c,
 * poweroff.c, kbdmap.c, setarg.c, ifarg.c, listarg.c, say.c
 */

/*
 * Based on busybox code.
 *
 * Compute MD5 checksum of strings according to the
 * definition of MD5 in RFC 1321 from April 1992.
 *
 * Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.
 *
 * Copyright (C) 1995-1999 Free Software Foundation, Inc.
 * Copyright (C) 2001 Manuel Novoa III
 * Copyright (C) 2003 Glenn L. McGrath
 * Copyright (C) 2003 Erik Andersen
 * Copyright (C) 2010 Denys Vlasenko
 * Copyright (C) 2012 Pascal Bellard
 *
 * Licensed under GPLv2 or later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <console.h>
#include <com32.h>

#define ALIGN1

static uint8_t wbuffer[64]; /* always correctly aligned for uint64_t */
static uint64_t total64;    /* must be directly before hash[] */
static uint32_t hash[8];    /* 4 elements for md5, 5 for sha1, 8 for sha256 */

/* Emit a string of hex representation of bytes */
static char* bin2hex(char *p)
{
	static const char bb_hexdigits_upcase[] ALIGN1 = "0123456789abcdef";
	int count = 16;
	const char *cp = (const char *) hash;
	while (count) {
		unsigned char c = *cp++;
		/* put lowercase hex digits */
		*p++ = bb_hexdigits_upcase[c >> 4];
		*p++ = bb_hexdigits_upcase[c & 0xf];
		count--;
	}
	return p;
}

//#define rotl32(x,n) (((x) << (n)) | ((x) >> (32 - (n))))
static uint32_t rotl32(uint32_t x, unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

static void md5_process_block64(void);

/* Feed data through a temporary buffer.
 * The internal buffer remembers previous data until it has 64
 * bytes worth to pass on.
 */
static void common64_hash(const void *buffer, size_t len)
{
	unsigned bufpos = total64 & 63;

	total64 += len;

	while (1) {
		unsigned remaining = 64 - bufpos;
		if (remaining > len)
			remaining = len;
		/* Copy data into aligned buffer */
		memcpy(wbuffer + bufpos, buffer, remaining);
		len -= remaining;
		buffer = (const char *)buffer + remaining;
		bufpos += remaining;
		/* clever way to do "if (bufpos != 64) break; ... ; bufpos = 0;" */
		bufpos -= 64;
		if (bufpos != 0)
			break;
		/* Buffer is filled up, process it */
		md5_process_block64();
		/*bufpos = 0; - already is */
	}
}

/* Process the remaining bytes in the buffer */
static void common64_end(void)
{
	unsigned bufpos = total64 & 63;
	/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0... */
	wbuffer[bufpos++] = 0x80;

	/* This loop iterates either once or twice, no more, no less */
	while (1) {
		unsigned remaining = 64 - bufpos;
		memset(wbuffer + bufpos, 0, remaining);
		/* Do we have enough space for the length count? */
		if (remaining >= 8) {
			/* Store the 64-bit counter of bits in the buffer */
			uint64_t t = total64 << 3;
			/* wbuffer is suitably aligned for this */
			*(uint64_t *) (&wbuffer[64 - 8]) = t;
		}
		md5_process_block64();
		if (remaining >= 8)
			break;
		bufpos = 0;
	}
}

/* These are the four functions used in the four steps of the MD5 algorithm
 * and defined in the RFC 1321.  The first function is a little bit optimized
 * (as found in Colin Plumbs public domain implementation).
 * #define FF(b, c, d) ((b & c) | (~b & d))
 */
#undef FF
#undef FG
#undef FH
#undef FI
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF(d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* Hash a single block, 64 bytes long and 4-byte aligned */
static void md5_process_block64(void)
{
	/* Before we start, one word to the strange constants.
	   They are defined in RFC 1321 as
	   T[i] = (int)(4294967296.0 * fabs(sin(i))), i=1..64
	 */
	static const uint32_t C_array[] = {
		/* round 1 */
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		/* round 2 */
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		/* round 3 */
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		/* round 4 */
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};
	static const char P_array[] ALIGN1 = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, /* 1 */
		1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, /* 2 */
		5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, /* 3 */
		0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9  /* 4 */
	};
	uint32_t *words = (void*) wbuffer;
	uint32_t A = hash[0];
	uint32_t B = hash[1];
	uint32_t C = hash[2];
	uint32_t D = hash[3];

	static const char S_array[] ALIGN1 = {
		7, 12, 17, 22,
		5, 9, 14, 20,
		4, 11, 16, 23,
		6, 10, 15, 21
	};
	const uint32_t *pc;
	const char *pp;
	const char *ps;
	int i;
	uint32_t temp;


	pc = C_array;
	pp = P_array;
	ps = S_array - 4;

	for (i = 0; i < 64; i++) {
		if ((i & 0x0f) == 0)
			ps += 4;
		temp = A;
		switch (i >> 4) {
		case 0:
			temp += FF(B, C, D);
			break;
		case 1:
			temp += FG(B, C, D);
			break;
		case 2:
			temp += FH(B, C, D);
			break;
		case 3:
			temp += FI(B, C, D);
		}
		temp += words[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}
	/* Add checksum to the starting values */
	hash[0] += A;
	hash[1] += B;
	hash[2] += C;
	hash[3] += D;

}
#undef FF
#undef FG
#undef FH
#undef FI

/* Initialize structure containing state of computation.
 * (RFC 1321, 3.3: Step 3)
 */
static void md5_begin(void)
{
	hash[0] = 0x67452301;
	hash[1] = 0xefcdab89;
	hash[2] = 0x98badcfe;
	hash[3] = 0x10325476;
	total64 = 0;
}

/* Used also for sha1 and sha256 */
#define md5_hash common64_hash

/* Process the remaining bytes in the buffer and put result from CTX
 * in first 16 bytes following RESBUF.  The result is always in little
 * endian byte order, so that a byte-wise output yields to the wanted
 * ASCII representation of the message digest.
 */
#define md5_end common64_end

/*
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003-2004 Erik Andersen
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

static char *unrockridge(const char *name)
{
	static char buffer[256];
	int i = 0, j = 8;
	while (*name && i < 255) {
		char c = *name++;
		//if (c == '\\') c = '/';
		if (c == '.') {
			for (j = i; --j >= 0 && buffer[j] != '/';)
				if (buffer[j] == '.') buffer[j] = '_';
			if (i - j > 9) i = j + 9;
			j = i + 4;
		}
		else if (c == '/') j = i + 9;
		else if (i >= j) continue;
		else if (c >= 'a' && c <= 'z') c += 'A' - 'a';
		else if ((c < 'A' || c > 'Z') && (c < '0' || c > '9')) c = '_';
		buffer[i++] = c;
	}
	buffer[i] = 0;
	return buffer;
}

static uint8_t *hash_file(const char *filename)
{
	int src_fd, count;
	uint8_t in_buf[4096];
	static uint8_t hash_value[16*2+1];

	src_fd = open(filename, O_RDONLY);
	if (src_fd < 0) {
		src_fd = open(unrockridge(filename), O_RDONLY);
	}
	if (src_fd < 0) {
		return NULL;
	}

	md5_begin();
	while ((count = read(src_fd, in_buf, 4096)) > 0) {
		md5_hash(in_buf, count);
	}

	close(src_fd);
	
	if (count)
		return NULL;

	md5_end();
	bin2hex((char *)hash_value);

	return hash_value;
}

static int main_say(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		printf("%s ",argv[i]);
	}
	sleep(5);
	return 0;
}

static int main_md5sum(int argc, char **argv)
{
	int files = 0, tested = 0, good = 0;
	static char clear_eol[] = "                                ";

	(void) argc;
	/* -c implied */
	argv++;
	do {
		FILE *fp;
		char eol, *line, buffer[4096];
		fp = fopen(*argv,"r");
		if (fp == NULL)
			fp = fopen(unrockridge(*argv),"r");

		while ((line = fgets(buffer,sizeof(buffer),fp)) != NULL) {
			uint8_t *hash_value;
			char *filename_ptr, *status;
			int len = strlen(line);

			if (line[0] < '0')
				continue;
			if (line[len-1] == '\n')
				line[len-1] = 0;
			filename_ptr = strstr(line, "  ");
			/* handle format for binary checksums */
			if (filename_ptr == NULL) {
				filename_ptr = strstr(line, " *");
			}
			if (filename_ptr == NULL) {
				continue;
			}
			*filename_ptr = '\0';
			*++filename_ptr = '/';
			if (filename_ptr[1] == '/')
				filename_ptr++;

			files++;
			status = "NOT CHECKED";
			eol = '\n';
			hash_value = hash_file(filename_ptr);
			if (hash_value) {
				tested++;
				status = "BROKEN";
				if (!strcmp((char*)hash_value, line)) {
					good++;
					status = "OK";
					eol = ' ';
				}
			}
			printf("\r%s: %s%s%c", filename_ptr, status, clear_eol, eol);
		}
		fclose(fp);
	} while (*++argv);
	printf("\r%d files OK, %d broken, %d not checked.%s\n",
		good, tested - good, files - tested, clear_eol);
	sleep(5);
	return 0;
}

/*
 * ifmem.c
 *
 * Run one command if the memory is large enought, and another if it isn't.
 *
 * Usage:
 *
 *    label boot_kernel
 *        kernel ifmem.c
 *        append size_in_KB boot_large [size_in_KB boot_medium] boot_small
 *
 *    label boot_large
 *        kernel vmlinuz_large_memory
 *        append ...
 *
 *    label boot_small
 *        kernel vmlinuz_small_memory
 *        append ...
 */

#include <inttypes.h>
#include <alloca.h>
#include <syslinux/boot.h>

struct e820_data {
	uint64_t base;
	uint64_t len;
	uint32_t type;
	uint32_t extattr;
} __attribute__((packed));

// Get memory size in Kb
static unsigned long memory_size(void)
{
	uint64_t bytes = 0;
	static com32sys_t ireg, oreg;
	static struct e820_data ed;

	ireg.eax.w[0] = 0xe820;
	ireg.edx.l    = 0x534d4150;
	ireg.ecx.l    = sizeof(struct e820_data);
	ireg.edi.w[0] = OFFS(__com32.cs_bounce);
	ireg.es       = SEG(__com32.cs_bounce);

	ed.extattr = 1;

	do {
		memcpy(__com32.cs_bounce, &ed, sizeof ed);

		__intcall(0x15, &ireg, &oreg);
		if (oreg.eflags.l & EFLAGS_CF ||
		    oreg.eax.l != 0x534d4150  ||
		    oreg.ecx.l < 20)
			break;

		memcpy(&ed, __com32.cs_bounce, sizeof ed);

		if (ed.type == 1)
			bytes += ed.len;

		ireg.ebx.l = oreg.ebx.l;
	} while (ireg.ebx.l);

	if (!bytes) {
		memset(&ireg, 0, sizeof ireg);
		ireg.eax.w[0] = 0x8800;
		__intcall(0x15, &ireg, &oreg);
		return ireg.eax.w[0];
	}
	return bytes >> 10;
}

static void usage(const char *msg)
{
	fprintf(stderr,"\n%s\n.",msg);
	sleep(5);
	exit(1);
}

static int main_ifmem(int argc, char *argv[])
{
	int i;
	unsigned long ram_size;

	if (argc < 4) {
		usage("Usage: ifmem.c32 size_KB boot_large_memory boot_small_memory");
	}

	// find target according to ram size
	ram_size = memory_size();
	printf("Total memory found %luK.\n",ram_size);
	ram_size += (1 << 10); // add 1M to round boundaries...
  
	i = 1;
	do { 
		char *s = argv[i];
		char *p = s;
		unsigned long scale = 1;

		while (*p >= '0' && *p <= '9') p++;
		switch (*p | 0x20) {
			case 'g': scale <<= 10;
			case 'm': scale <<= 10;
			default : *p = 0; break;
		}
		i++; // seek to label
		if (ram_size >= scale * strtoul(s, NULL, 0)) break;
		i++; // next size or default label
	} while (i + 1 < argc);

	if (i != argc)	syslinux_run_command(argv[i]);
	else		syslinux_run_default();
	return -1;
}

#include <syslinux/reboot.h>

static int main_reboot(int argc, char *argv[])
{
    int warm = 0;
    int i;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--warm"))
	    warm = 1;
    }

    syslinux_reboot(warm);
}

/* APM poweroff module.
 * based on poweroff.asm,  Copyright 2009 Sebastian Herbszt
 */

static int main_poweroff(int argc, char *argv[])
{
	static com32sys_t ireg, oreg;
	static char notsupported[] ="APM 1.1+ not supported";
	unsigned i;
	static struct {
		unsigned short ax;
		unsigned short bx;
		unsigned short cx;
		char *msg;
	} inst[] = {
		{ 0x5300,	// APM Installation Check (00h)
		  0, 		// APM BIOS (0000h)
		  0, "APM not present" },
		{ 0x5301,	// APM Real Mode Interface Connect (01h)
		  0, 		// APM BIOS (0000h)
		  0, "APM RM interface connect failed" },
		{ 0x530E,	// APM Driver Version (0Eh)
		  0, 		// APM BIOS (0000h)
		  0x0101,	// APM Driver Version version 1.1
		  notsupported },
		{ 0x5307,	// Set Power State (07h)
		  1,		// All devices power managed by the APM
		  3,		// Power state off
		  "Power off failed" }
	};

	(void) argc;
	(void) argv;
	for (i = 0; i < sizeof(inst)/sizeof(inst[0]); i++) {
		char *msg = inst[i].msg;
		
		ireg.eax.w[0] = inst[i].ax;
		ireg.ebx.w[0] = inst[i].bx;
		ireg.ecx.w[0] = inst[i].cx;
		__intcall(0x15, &ireg, &oreg);
		if ((oreg.eflags.l & EFLAGS_CF) == 0) {
			switch (inst[i].ax) {
			case 0x5300 :
				if (oreg.ebx.w[0] != 0x504D /* 'PM' */) break;
				msg = "Power management disabled";
				if (oreg.ecx.w[0] & 8) break; // bit 3 APM BIOS Power Management disabled
			case 0x530E :
				msg = notsupported;
				if (oreg.eax.w[0] < 0x101) break;
			default : continue;
			}
		}
		printf("%s.\n", msg);
		return 1;
	}
	return 0;
}

/*
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 */

#include <syslinux/keyboard.h>
#include <syslinux/loadfile.h>
#include <syslinux/adv.h>

static void setlinuxarg(int slot, int argc, char *argv[])
{
	for (; argc--; argv++)
		syslinux_setadv(slot++, strlen(*argv), *argv);
}

static int main_kbdmap(int argc, char *argv[])
{
    const struct syslinux_keyboard_map *const kmap = syslinux_keyboard_map();
    size_t map_size, size, i;
    char *kbdmap, *msg;

    if (argc < 3)
	usage("Usage: kbdmap archive.cpio mapfile [cmdline]..");

    // Save extra cmdline arguments
    setlinuxarg(1, argc - 3, argv + 3);

    msg="Append to kernel parameters: ";
    for (i = 3; i < (size_t) argc; i++, msg = " ")
	printf("%s%s",msg,argv[i]);
    printf("\n\n                            Hit RETURN to continue.\n");

    msg = "Load error";
    if (kmap->version != 1 ||
	loadfile(argv[1], (void **) &kbdmap, &map_size) || 
	strncmp(kbdmap, "07070", 5))
    	goto kbdmap_error;

    // search for mapfile in cpio archive
    for (i = 0; i < map_size;) {
	int len, j;
	char *name;
	
	for (j = size = 0; j < 8; j++) {
		char c = kbdmap[54 + i + j] - '0';
		if (c > 9) c += '0' + 10 - 'A';
		size <<= 4;
		size += c;
	}
	i += 110;
	name = kbdmap + i;
	len = 1 + strlen(name);
	i += len;
	i += ((-i)&3);
	if (!strcmp(name, argv[2])) {
	    kbdmap += i;
	    break;
	}
	i += size + ((-size)&3);
    }

    msg = "Filename error";
    if (i >= map_size)
    	goto kbdmap_error;

    msg = "Format error";
    if (size != kmap->length)
    	goto kbdmap_error;

    memcpy(kmap->map, kbdmap, size);

    return 0;

kbdmap_error:
    printf("%s.\n",msg);
    return 1;
}

/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2012 Intel Corporation; author: H. Peter Anvin
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

#include <errno.h>
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
    size_t bytes, size;
    char *cmdline, *p;
    int i;

    bytes = 1;			/* Just in case we have a zero-entry cmdline */
    for (arg = argv; *arg; arg++) {
	bytes += strlen(*arg) + 1;
    }
    for (i = 0; i < 255; i++)
    	if (syslinux_getadv(i, &size))
    		bytes += ++size;
  
    p = cmdline = malloc(bytes);
    if (!cmdline)
	return NULL;

    for (arg = argv; *arg; arg++) {
	int len = strlen(*arg);
	memcpy(p, *arg, len);
	p[len] = ' ';
	p += len + 1;
    }

    for (i = 0; i < 255; i++) {
    	const void *q = syslinux_getadv(i, &size);
    	if (q == NULL) continue;
    	memcpy(p, q, size);
	p[size] = ' ';
	p += size + 1;
    }

    if (p > cmdline)
	p--;			/* Remove the last space */
    *p = '\0';

    return cmdline;
}

static int setup_data_file(struct setup_data *setup_data,
			   uint32_t type, const char *filename,
			   bool opt_quiet)
{
    if (!opt_quiet)
	printf("Loading %s... ", filename);

    if (setup_data_load(setup_data, type, filename)) {
	if (opt_quiet)
	    printf("Loading %s ", filename);
	printf("failed\n");
	return -1;
    }
	    
    if (!opt_quiet)
	printf("ok\n");
    
    return 0;
}

static int main_linux(int argc, char *argv[])
{
    const char *kernel_name;
    struct initramfs *initramfs;
    struct setup_data *setup_data;
    char *cmdline;
    char *boot_image;
    void *kernel_data;
    size_t kernel_len;
    bool opt_dhcpinfo = false;
    bool opt_quiet = false;
    void *dhcpdata;
    size_t dhcplen;
    char **argp, **argl, *arg, *p;

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

    errno = 0;
    boot_image = malloc(strlen(kernel_name) + 12);
    if (!boot_image) {
	fprintf(stderr, "Error allocating BOOT_IMAGE string: ");
	goto bail;
    }
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
    errno = 0;
    if (loadfile(kernel_name, &kernel_data, &kernel_len)) {
	if (opt_quiet)
	    printf("Loading %s ", kernel_name);
	printf("failed: ");
	goto bail;
    }
    if (!opt_quiet)
	printf("ok\n");

    errno = 0;
    cmdline = make_cmdline(argp);
    if (!cmdline) {
	fprintf(stderr, "make_cmdline() failed: ");
	goto bail;
    }

    /* Initialize the initramfs chain */
    errno = 0;
    initramfs = initramfs_init();
    if (!initramfs) {
	fprintf(stderr, "initramfs_init() failed: ");
	goto bail;
    }

    if ((arg = find_argument(argp, "initrd="))) {
	do {
	    p = strchr(arg, ',');
	    if (p)
		*p = '\0';

	    if (!opt_quiet)
		printf("Loading %s... ", arg);
	    errno = 0;
	    if (initramfs_load_archive(initramfs, arg)) {
		if (opt_quiet)
		    printf("Loading %s ", kernel_name);
		printf("failed: ");
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
	errno = 0;
	if (initramfs_add_file(initramfs, dhcpdata, dhcplen, dhcplen,
			       "/dhcpinfo.dat", 0, 0755)) {
	    fprintf(stderr, "Unable to add DHCP info: ");
	    goto bail;
	}
    }

    /* Handle dtb and eventually other setup data */
    setup_data = setup_data_init();
    if (!setup_data)
	goto bail;

    for (argl = argv; (arg = *argl); argl++) {
	if (!memcmp(arg, "dtb=", 4)) {
	    if (setup_data_file(setup_data, SETUP_DTB, arg+4, opt_quiet))
		goto bail;
	} else if (!memcmp(arg, "blob.", 5)) {
	    uint32_t type;
	    char *ep;

	    type = strtoul(arg + 5, &ep, 10);
	    if (ep[0] != '=' || !ep[1])
		continue;

	    if (!type)
		continue;

	    if (setup_data_file(setup_data, type, ep+1, opt_quiet))
		goto bail;
	}
    }

    /* This should not return... */
    errno = 0;
    syslinux_boot_linux(kernel_data, kernel_len, initramfs,
			setup_data, cmdline);
    fprintf(stderr, "syslinux_boot_linux() failed: ");

bail:
    switch(errno) {
    case ENOENT:
	fprintf(stderr, "File not found\n");
	break;
    case ENOMEM:
	fprintf(stderr, "Out of memory\n");
	break;
    default:
	fprintf(stderr, "Error %d\n", errno);
	break;
    }
    fprintf(stderr, "%s: Boot aborted!\n", progname);
    return 1;
}

static int main_setarg(int argc, char *argv[])
{
	if (argc < 3) {
		usage("Usage: setarg.c32 argnum [args]...");
	}
	setlinuxarg(atoi(argv[1]), argc - 2, argv + 2);
	return 0;
}

static int main_ifarg(int argc, char *argv[])
{
	int i;
	size_t size;

	if (argc < 3) {
		usage("Usage: ifarg.c32 [argnum labelifset]... labelifnoneset");
	}
	for (i = 1; i < argc - 1; i += 2) {
		int n = atoi(argv[i]);
		if (n == -1) {
			for (n = 0; n < 255; n++) {
				if (syslinux_getadv(n, &size))
					goto found;
			}
			continue;
		}
		else if (! syslinux_getadv(n, &size)) continue;
	found:
		syslinux_run_command(argv[i+1]);
	}
	if (i != argc)  syslinux_run_command(argv[i]);
	else		syslinux_run_default();
	return 0;
}

/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

static int main_listarg(int argc, char *argv[])
{
    uint8_t *p, *ep;
    size_t s = syslinux_adv_size();
    char buf[256];

    (void) argc;
    (void) argv;
    p = syslinux_adv_ptr();

    printf("args size: %zd bytes at %p\n", s, p);

    ep = p + s;			/* Need at least opcode+len */
    while (p < ep - 1 && *p) {
	int t = *p++;
	int l = *p++;

	if (p + l > ep)
	    break;

	memcpy(buf, p, l);
	buf[l] = '\0';

	printf("arg %3d: \"%s\"\n", t, buf);

	p += l;
    }
    sleep(5);
    return 0;
}

int main(int argc, char *argv[])
{
	unsigned i;
	static struct {
		char *name;
		int (*main)(int argc, char *argv[]);
	} bin[] = {
		{ "say",	main_say      },
		{ "md5sum",	main_md5sum   },
		{ "ifmem",	main_ifmem    },
		{ "reboot",	main_reboot   },
		{ "poweroff",	main_poweroff },
		{ "kbdmap",	main_kbdmap   },
		{ "linux",	main_linux    },
		{ "setarg",	main_setarg   },
		{ "ifarg",	main_ifarg    },
		{ "listarg",	main_listarg  }
	};

	openconsole(&dev_null_r, &dev_stdcon_w);
  
	if (strstr(argv[0], "c32box")) { argc--; argv++; }
	for (i = 0; i < sizeof(bin)/sizeof(bin[0]); i++)
		if (strstr(argv[0], bin[i].name))
			return bin[i].main(argc, argv);
	printf("No %s in c32box modules\n", argv[0]);
	for (i = 0; i < sizeof(bin)/sizeof(bin[0]); i++)
		printf("  %s \n",bin[i].name);
	return 1;
}
