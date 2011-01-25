/*
 * isohybrid.c: Post process an ISO 9660 image generated with mkisofs or
 * genisoimage to allow - hybrid booting - as a CD-ROM or as a hard
 * disk.
 *
 * This is based on the original Perl script written by H. Peter Anvin. The
 * rewrite in C is to avoid dependency on Perl on a system under installation.
 *
 * Copyright (C) 2010 P J P <pj.pandit@yahoo.co.in>
 *
 * isohybrid is a free software; you can redistribute it and/or modify it
 * under the terms of GNU General Public License as published by Free Software
 * Foundation; either version 2 of the license, or (at your option) any later
 * version.
 *
 * isohybrid is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with isohybrid; if not, see: <http://www.gnu.org/licenses>.
 *
 */

#include <err.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <alloca.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "isohybrid.h"

char *prog = NULL;
extern int opterr, optind;

uint8_t mode = 0;
enum { VERBOSE = 1 };

/* user options */
uint16_t head = 64;             /* 1 <= head <= 256 */
uint8_t sector = 32;            /* 1 <= sector <= 63  */

uint8_t entry = 1;              /* partition number: 1 <= entry <= 4 */
uint8_t offset = 0;             /* partition offset: 0 <= offset <= 64 */
uint16_t type = 0x17;           /* partition type: 0 <= type <= 255 */
uint32_t id = 0;                /* MBR: 0 <= id <= 0xFFFFFFFF(4294967296) */

uint8_t hd0 = 0;                /* 0 <= hd0 <= 2 */
uint8_t partok = 0;             /* 0 <= partok <= 1 */

uint16_t ve[16];
uint32_t catoffset = 0;
uint32_t c = 0, cc = 0, cs = 0;

/* boot catalogue parameters */
uint32_t de_lba = 0;
uint16_t de_seg = 0, de_count = 0, de_mbz2 = 0;
uint8_t de_boot = 0, de_media = 0, de_sys = 0, de_mbz1 = 0;


void
usage(void)
{
    printf("Usage: %s [OPTIONS] <boot.iso>\n", prog);
}


void
printh(void)
{
#define FMT "%-18s %s\n"

    usage();

    printf("\n");
    printf("Options:\n");
    printf(FMT, "   -h <X>", "Number of default geometry heads");
    printf(FMT, "   -s <X>", "Number of default geometry sectors");
    printf(FMT, "   -e --entry", "Specify partition entry number (1-4)");
    printf(FMT, "   -o --offset", "Specify partition offset (default 0)");
    printf(FMT, "   -t --type", "Specify partition type (default 0x17)");
    printf(FMT, "   -i --id", "Specify MBR ID (default random)");

    printf("\n");
    printf(FMT, "   --forcehd0", "Assume we are loaded as disk ID 0");
    printf(FMT, "   --ctrlhd0", "Assume disk ID 0 if the Ctrl key is pressed");
    printf(FMT, "   --partok", "Allow booting from within a partition");

    printf("\n");
    printf(FMT, "   -? --help", "Display this help");
    printf(FMT, "   -v --verbose", "Display verbose output");
    printf(FMT, "   -V --version", "Display version information");

    printf("\n");
    printf("Report bugs to <pj.pandit@yahoo.co.in>\n");
}


int
check_option(int argc, char *argv[])
{
    int n = 0, ind = 0;

    const char optstr[] = ":h:s:e:o:t:i:fcp?vV";
    struct option lopt[] = \
    {
        { "entry", required_argument, NULL, 'e' },
        { "offset", required_argument, NULL, 'o' },
        { "type", required_argument, NULL, 't' },
        { "id", required_argument, NULL, 'i' },

        { "forcehd0", no_argument, NULL, 'f' },
        { "ctrlhd0", no_argument, NULL, 'c' },
        { "partok", no_argument, NULL, 'p'},

        { "help", no_argument, NULL, '?' },
        { "verbose", no_argument, NULL, 'v' },
        { "version", no_argument, NULL, 'V' },

        { 0, 0, 0, 0 }
    };

    opterr = mode = 0;
    while ((n = getopt_long_only(argc, argv, optstr, lopt, &ind)) != -1)
    {
        switch (n)
        {
        case 'h':
            if (!sscanf(optarg, "%hu", &head) || head < 1 || head > 256)
                errx(1, "invalid head: `%s', 1 <= head <= 256", optarg);
            break;

        case 's':
            if (!sscanf(optarg, "%hhu", &sector) || sector < 1 || sector > 63)
                errx(1, "invalid sector: `%s', 1 <= sector <= 63", optarg);
            break;

        case 'e':
            if (!sscanf(optarg, "%hhu", &entry) || entry < 1 || entry > 4)
                errx(1, "invalid entry: `%s', 1 <= entry <= 4", optarg);
            break;

        case 'o':
            if (!sscanf(optarg, "%hhu", &offset) || offset > 64)
                errx(1, "invalid offset: `%s', 0 <= offset <= 64", optarg);
            break;

        case 't':
            if (!sscanf(optarg, "%hu", &type) || type > 255)
                errx(1, "invalid type: `%s', 0 <= type <= 255", optarg);
            break;

        case 'i':
            if (!sscanf(optarg, "%u", &id))
                errx(1, "invalid id: `%s'", optarg);
            break;

        case 'f':
            hd0 = 1;
            break;

        case 'c':
            hd0 = 2;
            break;

        case 'p':
            partok = 1;
            break;

        case 'v':
            mode |= VERBOSE;
            break;

        case 'V':
            printf("%s version %s\n", prog, VERSION);
            exit(0);

        case ':':
            errx(1, "option `-%c' takes an argument", optopt);

        default:
        case '?':
            if (optopt)
                errx(1, "invalid option `-%c', see --help", optopt);

            printh();
            exit(0);
        }
    }

    return optind;
}


uint16_t
lendian_short(const uint16_t s)
{
    uint16_t r = 1;

    if (*(uint8_t *)&r)
        return s;

    r = (s & 0x00FF) << 8 | (s & 0xFF00) >> 8;

    return r;
}


uint32_t
lendian_int(const uint32_t s)
{
    uint32_t r = 1;

    if (*(uint8_t *)&r)
        return s;

    r = (s & 0x000000FF) << 24 | (s & 0xFF000000) >> 24
        | (s & 0x0000FF00) << 8 | (s & 0x00FF0000) >> 8;

    return r;
}


int
check_banner(const uint8_t *buf)
{
    static const char banner[] = "\0CD001\1EL TORITO SPECIFICATION\0\0\0\0" \
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" \
        "\0\0\0\0\0";

    if (!buf || memcmp(buf, banner, sizeof(banner) - 1))
        return 1;

    buf += sizeof(banner) - 1;
    memcpy(&catoffset, buf, sizeof(catoffset));

    catoffset = lendian_int(catoffset);

    return 0;
}


int
check_catalogue(const uint8_t *buf)
{
    int i = 0;

    for (i = 0, cs = 0; i < 16; i++)
    {
        ve[i] = 0;
        memcpy(&ve[i], buf, sizeof(ve[i]));

        ve[i] = lendian_short(ve[i]);

        buf += 2;
        cs += ve[i];

        if (mode & VERBOSE)
            printf("ve[%d]: %d, cs: %d\n", i, ve[i], cs);
    }
    if ((ve[0] != 0x0001) || (ve[15] != 0xAA55) || (cs & 0xFFFF))
        return 1;

    return 0;
}


int
read_catalogue(const uint8_t *buf)
{
    memcpy(&de_boot, buf++, 1);
    memcpy(&de_media, buf++, 1);

    memcpy(&de_seg, buf, 2);
    de_seg = lendian_short(de_seg);
    buf += 2;

    memcpy(&de_sys, buf++, 1);
    memcpy(&de_mbz1, buf++, 1);

    memcpy(&de_count, buf, 2);
    de_count = lendian_short(de_count);
    buf += 2;

    memcpy(&de_lba, buf, 4);
    de_lba = lendian_int(de_lba);
    buf += 4;

    memcpy(&de_mbz2, buf, 2);
    de_mbz2 = lendian_short(de_mbz2);
    buf += 2;

    if (de_boot != 0x88 || de_media != 0
        || (de_seg != 0 && de_seg != 0x7C0) || de_count != 4)
        return 1;

    return 0;
}


void
display_catalogue(void)
{
    printf("de_boot: %hhu\n", de_boot);
    printf("de_media: %hhu\n", de_media);
    printf("de_seg: %hu\n", de_seg);
    printf("de_sys: %hhu\n", de_sys);
    printf("de_mbz1: %hhu\n", de_mbz1);
    printf("de_count: %hu\n", de_count);
    printf("de_lba: %u\n", de_lba);
    printf("de_mbz2: %hu\n", de_mbz2);
}


int
initialise_mbr(uint8_t *mbr)
{
    int i = 0;
    uint32_t psize = 0, tmp = 0;
    uint8_t ptype = 0, *rbm = mbr;
    uint8_t bhead = 0, bsect = 0, bcyle = 0;
    uint8_t ehead = 0, esect = 0, ecyle = 0;

    extern unsigned char isohdpfx[][MBRSIZE];

    memcpy(mbr, &isohdpfx[hd0 + 3 * partok], MBRSIZE);
    mbr += MBRSIZE;                                 /* offset 432 */

    tmp = lendian_int(de_lba * 4);
    memcpy(mbr, &tmp, sizeof(tmp));
    mbr += sizeof(tmp);                             /* offset 436 */

    tmp = 0;
    memcpy(mbr, &tmp, sizeof(tmp));
    mbr += sizeof(tmp);                             /* offset 440 */

    tmp = lendian_int(id);
    memcpy(mbr, &tmp, sizeof(tmp));
    mbr += sizeof(tmp);                             /* offset 444 */

    mbr[0] = '\0';
    mbr[1] = '\0';
    mbr += 2;                                       /* offset 446 */

    ptype = type;
    psize = c * head * sector - offset;

    bhead = (offset / sector) % head;
    bsect = (offset % sector) + 1;
    bcyle = offset / (head * sector);

    bsect += (bcyle & 0x300) >> 2;
    bcyle  &= 0xFF;

    ehead = head - 1;
    esect = sector + (((cc - 1) & 0x300) >> 2);
    ecyle = (cc - 1) & 0xFF;

    for (i = 1; i <= 4; i++)
    {
        memset(mbr, 0, 16);
        if (i == entry)
        {
            mbr[0] = 0x80;
            mbr[1] = bhead;
            mbr[2] = bsect;
            mbr[3] = bcyle;
            mbr[4] = ptype;
            mbr[5] = ehead;
            mbr[6] = esect;
            mbr[7] = ecyle;

            tmp = lendian_int(offset);
            memcpy(&mbr[8], &tmp, sizeof(tmp));

            tmp = lendian_int(psize);
            memcpy(&mbr[12], &tmp, sizeof(tmp));
        }
        mbr += 16;
    }
    mbr[0] = 0x55;
    mbr[1] = 0xAA;
    mbr += 2;

    return mbr - rbm;
}


void
display_mbr(const uint8_t *mbr, size_t len)
{
    unsigned char c = 0;
    unsigned int i = 0, j = 0;

    printf("sizeof(MBR): %zu bytes\n", len);
    for (i = 0; i < len; i++)
    {
        if (!(i % 16))
            printf("%04d ", i);

        if (!(i % 8))
            printf(" ");

        c = mbr[i];
        printf("%02x ", c);

        if (!((i + 1) % 16))
        {
            printf(" |");
            for (; j <= i; j++)
                printf("%c", isprint(mbr[j]) ? mbr[j] : '.');
            printf("|\n");
        }
    }
}


int
main(int argc, char *argv[])
{
    int i = 0;
    FILE *fp = NULL;
    struct stat isostat;
    uint8_t *buf = NULL, *bufz = NULL;
    int cylsize = 0, frac = 0, padding = 0;

    prog = strcpy(alloca(strlen(argv[0]) + 1), argv[0]);
    i = check_option(argc, argv);
    argc -= i;
    argv += i;

    if (!argc)
    {
        usage();
        return 1;
    }
    srand(time(NULL) << (getppid() << getpid()));

    if (!(fp = fopen(argv[0], "r+")))
        err(1, "could not open file `%s'", argv[0]);
    if (fseek(fp, 17 * 2048, SEEK_SET))
        err(1, "%s: seek error - 1", argv[0]);

    bufz = buf = calloc(BUFSIZE, sizeof(char));
    if (fread(buf, sizeof(char), BUFSIZE, fp) != BUFSIZE)
        err(1, "%s", argv[0]);

    if (check_banner(buf))
        errx(1, "%s: could not find boot record", argv[0]);

    if (mode & VERBOSE)
        printf("catalogue offset: %d\n", catoffset);

    if (fseek(fp, catoffset * 2048, SEEK_SET))
        err(1, "%s: seek error - 2", argv[0]);

    buf = bufz;
    memset(buf, 0, BUFSIZE);
    if (fread(buf, sizeof(char), BUFSIZE, fp) != BUFSIZE)
        err(1, "%s", argv[0]);

    if (check_catalogue(buf))
        errx(1, "%s: invalid boot catalogue", argv[0]);

    buf += sizeof(ve);
    if (read_catalogue(buf))
        errx(1, "%s: unexpected boot catalogue parameters", argv[0]);

    if (mode & VERBOSE)
        display_catalogue();

    if (fseek(fp, (de_lba * 2048 + 0x40), SEEK_SET))
        err(1, "%s: seek error - 3", argv[0]);

    buf = bufz;
    memset(buf, 0, BUFSIZE);
    if (fread(buf, sizeof(char), 4, fp) != 4)
        err(1, "%s", argv[0]);

    if (memcmp(buf, "\xFB\xC0\x78\x70", 4))
        errx(1, "%s: boot loader does not have an isolinux.bin hybrid " \
                 "signature. Note that isolinux-debug.bin does not support " \
                 "hybrid booting", argv[0]);

    if (stat(argv[0], &isostat))
        err(1, "%s", argv[0]);

    cylsize = head * sector * 512;
    frac = isostat.st_size % cylsize;
    padding = (frac > 0) ? cylsize - frac : 0;

    if (mode & VERBOSE)
        printf("imgsize: %zu, padding: %d\n", (size_t)isostat.st_size, padding);

    cc = c = (isostat.st_size + padding) / cylsize;
    if (c > 1024)
    {
        warnx("Warning: more than 1024 cylinders: %d", c);
        warnx("Not all BIOSes will be able to boot this device");
        cc = 1024;
    }

    if (!id)
    {
        if (fseek(fp, 440, SEEK_SET))
            err(1, "%s: seek error - 4", argv[0]);

	if (fread(&id, 1, 4, fp) != 4)
	    err(1, "%s: read error", argv[0]);

        id = lendian_int(id);
        if (!id)
        {
            if (mode & VERBOSE)
                printf("random ");
            id = rand();
        }
    }
    if (mode & VERBOSE)
        printf("id: %u\n", id);

    buf = bufz;
    memset(buf, 0, BUFSIZE);
    i = initialise_mbr(buf);

    if (mode & VERBOSE)
        display_mbr(buf, i);

    if (fseek(fp, 0, SEEK_SET))
        err(1, "%s: seek error - 5", argv[0]);

    if (fwrite(buf, sizeof(char), i, fp) != (size_t)i)
        err(1, "%s: write error - 1", argv[0]);

    if (padding)
    {
        if (fsync(fileno(fp)))
            err(1, "%s: could not synchronise", argv[0]);

        if (ftruncate(fileno(fp), isostat.st_size + padding))
            err(1, "%s: could not add padding bytes", argv[0]);
    }

    free(buf);
    fclose(fp);

    return 0;
}
