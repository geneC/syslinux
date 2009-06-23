#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <klibc/compiler.h>
#include "core.h"
#include "fs.h"
#include "disk.h"

#define RETRY_COUNT 6

extern uint16_t MaxTransfer;

static int chs_rdwr_sectors(struct disk *disk, void *buf,
			    sector_t lba, size_t count, bool is_write)
{
    char *ptr = buf;
    char *tptr;
    size_t chunk, freeseg;
    int sector_size  = disk->sector_size;
    int sector_shift = disk->sector_shift;
    uint32_t xlba = lba;	/* Truncated LBA (CHS is << 2 TB) */
    uint32_t t;
    uint16_t c, h, s;
    com32sys_t ireg, oreg;
    size_t done = 0;
    size_t bytes;
    int retry;

    memset(&ireg, 0, sizeof ireg);

    ireg.eax.b[1] = 0x02 + is_write;
    ireg.edx.b[0] = disk->disk_number;

    while (count) {
	chunk = count;
	if (chunk > MaxTransfer)
	    chunk = MaxTransfer;

	freeseg = (0x10000 - ((size_t)ptr & 0xffff)) >> sector_shift;

	if ((size_t)buf <= 0xf0000 && freeseg) {
	    /* Can do a direct load */
	    tptr = ptr;
	} else {
	    /* Either accessing high memory or we're crossing a 64K line */
	    tptr = core_xfer_buf;
	    freeseg = (0x10000 - ((size_t)tptr & 0xffff)) >> sector_shift;
	}
	if (chunk > freeseg)
	    chunk = freeseg;

	bytes = chunk << sector_shift;

	if (tptr != ptr && is_write)
	    memcpy(tptr, ptr, bytes);

	s = xlba % disk->s;
	t = xlba / disk->s;
	h = t % disk->h;
	c = t / disk->h;

	ireg.eax.b[0] = chunk;
	ireg.ecx.b[1] = c;
	ireg.ecx.b[0] = ((c & 0x300) >> 2) | (s+1);
	ireg.edx.b[1] = h;
	ireg.ebx.w[0] = OFFS(tptr);
	ireg.es       = SEG(tptr);

	retry = RETRY_COUNT;

	for (;;) {
	    __intcall(0x13, &ireg, &oreg);
	    if (!(oreg.eflags.l & EFLAGS_CF))
		break;
	    if (retry--)
		continue;
	    chunk >>= 1;
	    if (chunk) {
		MaxTransfer = chunk;
		retry = RETRY_COUNT;
		continue;
	    }
	    return done;	/* Failure */
	}

	bytes = chunk << sector_shift;

	if (tptr != ptr && !is_write)
	    memcpy(ptr, tptr, bytes);

	ptr   += bytes;
	xlba  += chunk;
	count -= chunk;
	done  += chunk;
    }
    return done;
}

struct edd_rdwr_packet {
    uint16_t size;
    uint16_t blocks;
    far_ptr_t buf;
    uint64_t lba;
};

static int edd_rdwr_sectors(struct disk *disk, void *buf,
			    sector_t lba, size_t count, bool is_write)
{
    static __lowmem struct edd_rdwr_packet pkt;
    char *ptr = buf;
    char *tptr;
    size_t chunk, freeseg;
    int sector_size  = disk->sector_size;
    int sector_shift = disk->sector_shift;
    com32sys_t ireg, oreg;
    size_t done = 0;
    size_t bytes;
    int retry;

    memset(&ireg, 0, sizeof ireg);

    ireg.eax.b[1] = 0x42 + is_write;
    ireg.edx.b[0] = disk->disk_number;
    ireg.ds       = SEG(&pkt);
    ireg.esi.w[0] = OFFS(&pkt);

    while (count) {
	chunk = count;
	if (chunk > MaxTransfer)
	    chunk = MaxTransfer;

	freeseg = (0x10000 - ((size_t)ptr & 0xffff)) >> sector_shift;

	if ((size_t)buf <= 0xf0000 && freeseg) {
	    /* Can do a direct load */
	    tptr = ptr;
	} else {
	    /* Either accessing high memory or we're crossing a 64K line */
	    tptr = core_xfer_buf;
	    freeseg = (0x10000 - ((size_t)tptr & 0xffff)) >> sector_shift;
	}
	if (chunk > freeseg)
	    chunk = freeseg;

	bytes = chunk << sector_shift;

	if (tptr != ptr && is_write)
	    memcpy(tptr, ptr, bytes);

	pkt.size   = sizeof pkt;
	pkt.blocks = chunk;
	pkt.buf    = FAR_PTR(tptr);
	pkt.lba    = lba;

	retry = RETRY_COUNT;

	for (;;) {
	    __intcall(0x13, &ireg, &oreg);
	    if (!(oreg.eflags.l & EFLAGS_CF))
		break;
	    if (retry--)
		continue;
	    chunk >>= 1;
	    if (chunk) {
		MaxTransfer = chunk;
		retry = RETRY_COUNT;
		continue;
	    }
	    /*** XXX: Consider falling back to CHS here?! ***/
	    return done;	/* Failure */
	}

	bytes = chunk << sector_shift;

	if (tptr != ptr && !is_write)
	    memcpy(ptr, tptr, bytes);

	ptr   += bytes;
	lba   += chunk;
	count -= chunk;
	done  += chunk;
    }
    return done;
}
struct edd_disk_params {
    uint16_t  len;
    uint16_t  flags;
    uint32_t  phys_c;
    uint32_t  phys_h;
    uint32_t  phys_s;
    uint64_t  sectors;
    uint16_t  sector_size;
    far_ptr_t dpte;
    uint16_t  devpath_key;
    uint8_t   devpath_len;
    uint8_t   _pad1[3];
    char      bus_type[4];
    char      if_type[8];
    uint8_t   if_path[8];
    uint8_t   dev_path[8];
    uint8_t   _pad2;
    uint8_t   devpath_csum;
} __attribute__((packed));

static inline bool is_power_of_2(uint32_t x)
{
    return !(x & (x-1));
}

int ilog2(uint32_t num)
{
    int i = 0;
    
    if (!is_power_of_2(num)) {
        printf("ERROR: the num must be power of 2 when conveting to log2\n");
        return 0;
    }
    while (num >>= 1)
        i++;
    return i;
}

void dump_disk(struct disk *disk)
{
    printf("drive number: 0x%x\n", disk->disk_number);
    printf("disk type: %s(%d)\n", disk->type ? "EDD" : "CHS", disk->type);
    printf("sector size: %d(%d)\n", disk->sector_size, disk->sector_shift);
    printf("h: %d\ts: %d\n", disk->h, disk->s);
    printf("offset: %d\n", disk->part_start);
    printf("%s\n", disk->rdwr_sectors == edd_rdwr_sectors ? "EDD_RDWR_SECTORS" :
           "CHS_RDWR_SECTORS");
    printf("--------------------------------\n");
    printf("disk->rdwr_sectors@: %p\n", disk->rdwr_sectors);
    printf("edd_rdwr_sectors  @: %p\n", edd_rdwr_sectors);
    printf("chs_rdwr_sectors  @: %p\n", chs_rdwr_sectors);
}

struct disk *disk_init(uint8_t devno, bool cdrom, sector_t part_start,
                       uint16_t bsHeads, uint16_t bsSecPerTrack)
{
    static struct disk disk;
    static __lowmem struct edd_disk_params edd_params;
    com32sys_t ireg, oreg;
    bool ebios = cdrom;
    int sector_size = cdrom ? 2048 : 512;

    memset(&ireg, 0, sizeof ireg);

    /* Get EBIOS support */
    ireg.eax.b[1] = 0x41;
    ireg.ebx.w[0] = 0x55aa;
    ireg.edx.b[0] = devno;
    ireg.eflags.b[0] = 0x3;	/* CF set */

    __intcall(0x13, &ireg, &oreg);

    if (cdrom || (!(oreg.eflags.l & EFLAGS_CF) &&
		  oreg.ebx.w[0] == 0xaa55 && (oreg.ecx.b[0] & 1))) {
	/* Query EBIOS parameters */
	ireg.eax.b[1] = 0x48;
	ireg.ds = SEG(&edd_params);
	ireg.esi.w[0] = OFFS(&edd_params);
	__intcall(0x13, &ireg, &oreg);

	if (!(oreg.eflags.l & EFLAGS_CF) && oreg.eax.b[1] == 0) {
	    ebios = true;
	    if (edd_params.sector_size >= 512 &&
		is_power_of_2(edd_params.sector_size))
		sector_size = edd_params.sector_size;
	}
    }

    /* CBIOS parameters */
    disk.h = bsHeads;
    disk.s = bsSecPerTrack;

    if ((int8_t)devno < 0) {
	/* Get hard disk geometry from BIOS */

	ireg.eax.b[1] = 0x08;
	__intcall(0x13, &ireg, &oreg);

	if (!(oreg.eflags.l & EFLAGS_CF)) {
	    disk.h = oreg.edx.b[1] + 1;
	    disk.s = oreg.ecx.b[0] & 63;
	}
    }

    disk.disk_number   = devno;
    disk.type          = ebios;
    disk.sector_size   = sector_size;
    disk.sector_shift  = ilog2(sector_size);
    disk.part_start    = part_start;
    disk.rdwr_sectors  = ebios ? edd_rdwr_sectors : chs_rdwr_sectors;

    dump_disk(&disk);

    return &disk;
}
