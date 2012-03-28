#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <core.h>

typedef uint64_t sector_t;
typedef uint64_t block_t;

#ifdef SYSLINUX_EFI
struct disk_private {
	EFI_HANDLE dev_handle;
	EFI_BLOCK_IO *bio;
	EFI_DISK_IO *dio;
};
#else
struct disk_private {
	com32sys_t *regs;
};
#endif

/*
 * struct disk: contains the information about a specific disk and also
 * contains the I/O function.
 */
struct disk {
    struct disk_private *private;	/* Firmware-private disk info */
    unsigned int disk_number;	/* in BIOS style */
    unsigned int sector_size;	/* gener512B or 2048B */
    unsigned int sector_shift;
    unsigned int maxtransfer;	/* Max sectors per transfer */
    
    unsigned int h, s;		/* CHS geometry */
    unsigned int secpercyl;	/* h*s */
    unsigned int _pad;

    sector_t part_start;   /* the start address of this partition(in sectors) */

    int (*rdwr_sectors)(struct disk *, void *, sector_t, size_t, bool);
};

extern void read_sectors(char *, sector_t, int);
extern void getoneblk(struct disk *, char *, block_t, int);

/* diskio.c */
struct disk *bios_disk_init(struct disk_private *);
struct device *device_init(struct disk_private *);

#endif /* DISK_H */
