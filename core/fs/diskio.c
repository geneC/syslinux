#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <klibc/compiler.h>
#include <core.h>
#include <fs.h>
#include <disk.h>

#define RETRY_COUNT 6

static int chs_rdwr_sectors(struct disk *disk, void *buf,
			    sector_t lba, size_t count, bool is_write)
{
    char *ptr = buf;
    char *tptr;
    size_t chunk, freeseg;
    int sector_shift = disk->sector_shift;
    uint32_t xlba = lba + disk->part_start; /* Truncated LBA (CHS is << 2 TB) */
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
	if (chunk > disk->maxtransfer)
	    chunk = disk->maxtransfer;

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
	    dprintf("CHS[%02x]: %u @ %llu (%u/%u/%u) %04x:%04x %s %p\n",
		    ireg.edx.b[0], chunk, xlba, c, h, s+1,
		    ireg.es, ireg.ebx.w[0],
		    (ireg.eax.b[1] & 1) ? "<-" : "->",
		    ptr);

	    __intcall(0x13, &ireg, &oreg);
	    if (!(oreg.eflags.l & EFLAGS_CF))
		break;
	    if (retry--)
		continue;

	    dprintf("CHS: error AX = %04x\n", oreg.eax.w[0]);

	    /* For any starting value, this will always end with ..., 1, 0 */
	    chunk >>= 1;
            if (chunk) {
		disk->maxtransfer = chunk;
		retry = RETRY_COUNT;
                ireg.eax.b[0] = chunk;
                continue;
	    } else {
		printf("CHS: Error %04x %s sector %llu (%u/%u/%u)\n",
		       oreg.eax.w[0],
		       is_write ? "writing" : "reading",
		       lba, c, h, s+1);
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

    lba += disk->part_start;
    while (count) {
	chunk = count;
	if (chunk > disk->maxtransfer)
	    chunk = disk->maxtransfer;

	freeseg = (0x10000 - ((size_t)ptr & 0xffff)) >> sector_shift;

	if ((size_t)ptr <= 0xf0000 && freeseg) {
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

	retry = RETRY_COUNT;

	for (;;) {
	    pkt.size   = sizeof pkt;
	    pkt.blocks = chunk;
	    pkt.buf    = FAR_PTR(tptr);
	    pkt.lba    = lba;

	    dprintf("EDD[%02x]: %u @ %llu %04x:%04x %s %p\n",
		    ireg.edx.b[0], pkt.blocks, pkt.lba,
		    pkt.buf.seg, pkt.buf.offs,
		    (ireg.eax.b[1] & 1) ? "<-" : "->",
		    ptr);

	    __intcall(0x13, &ireg, &oreg);
	    if (!(oreg.eflags.l & EFLAGS_CF))
		break;
	    if (retry--)
		continue;

	    dprintf("EDD: error AX = %04x\n", oreg.eax.w[0]);

	    /* For any starting value, this will always end with ..., 1, 0 */
	    chunk >>= 1;
	    if (chunk) {
		disk->maxtransfer = chunk;
		retry = RETRY_COUNT;
		continue;
	    }

	    /*** XXX: Consider falling back to CHS here?! ***/
            printf("EDD: Error %04x %s sector %llu\n",
		   oreg.eax.w[0],
		   is_write ? "writing" : "reading",
		   lba);
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

static int ilog2(uint32_t num)
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

void getoneblk(struct disk *disk, char *buf, block_t block, int block_size)
{
    int sec_per_block = block_size / disk->sector_size;

    disk->rdwr_sectors(disk, buf, block * sec_per_block, sec_per_block, 0);
}


struct disk *disk_init(uint8_t devno, bool cdrom, sector_t part_start,
                       uint16_t bsHeads, uint16_t bsSecPerTrack,
		       uint32_t MaxTransfer)
{
    static struct disk disk;
    static __lowmem struct edd_disk_params edd_params;
    com32sys_t ireg, oreg;
    bool ebios = cdrom;
    int sector_size = cdrom ? 2048 : 512;
    unsigned int hard_max_transfer = ebios ? 127 : 63;

    memset(&ireg, 0, sizeof ireg);

    /* Get EBIOS support */
    ireg.eax.b[1] = 0x41;
    ireg.ebx.w[0] = 0x55aa;
    ireg.edx.b[0] = devno;
    ireg.eflags.b[0] = 0x3;	/* CF set */

    __intcall(0x13, &ireg, &oreg);

    if (cdrom || (!(oreg.eflags.l & EFLAGS_CF) &&
		  oreg.ebx.w[0] == 0xaa55 && (oreg.ecx.b[0] & 1))) {
	ebios = true;
	hard_max_transfer = 127;

	/* Query EBIOS parameters */
	edd_params.len = sizeof edd_params;

	ireg.eax.b[1] = 0x48;
	ireg.ds = SEG(&edd_params);
	ireg.esi.w[0] = OFFS(&edd_params);
	__intcall(0x13, &ireg, &oreg);

	if (!(oreg.eflags.l & EFLAGS_CF) && oreg.eax.b[1] == 0) {
	    if (edd_params.len < sizeof edd_params)
		memset((char *)&edd_params + edd_params.len, 0,
		       sizeof edd_params - edd_params.len);

	    /*
	     * Note: filter impossible sector sizes.  Some BIOSes
	     * are known to report incorrect sector size information
	     * (usually 512 rather than 2048) for CD-ROMs, so at least
	     * for now ignore the reported sector size if booted via
	     * El Torito.
	     *
	     * Known affected systems: ThinkPad T22, T23.
	     */
	    if (!cdrom &&
		edd_params.sector_size >= 512 &&
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

    if (!MaxTransfer || MaxTransfer > hard_max_transfer)
	MaxTransfer = hard_max_transfer;

    disk.maxtransfer   = MaxTransfer;

    dprintf("disk %02x cdrom %d type %d sector %u/%u offset %llu\n",
	    devno, cdrom, ebios, sector_size, disk.sector_shift, part_start);

    return &disk;
}


/*
 * Initialize the device structure.
 *
 * NOTE: the disk cache needs to be revamped to support multiple devices...
 */
struct device * device_init(uint8_t devno, bool cdrom, sector_t part_start,
                            uint16_t bsHeads, uint16_t bsSecPerTrack,
			    uint32_t MaxTransfer)
{
    static struct device dev;
    static __hugebss char diskcache[128*1024];

    dev.disk = disk_init(devno, cdrom, part_start,
			 bsHeads, bsSecPerTrack, MaxTransfer);

    dev.cache_data = diskcache;
    dev.cache_size = sizeof diskcache;

    return &dev;
}
