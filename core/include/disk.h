#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint64_t sector_t;
typedef uint64_t block_t;

/*
 * struct disk: contains the information about a specific disk and also
 * contains the I/O function.
 */
struct disk {
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
struct disk *disk_init(uint8_t, bool, sector_t, uint16_t, uint16_t, uint32_t);
struct device *device_init(uint8_t, bool, sector_t, uint16_t, uint16_t, uint32_t);

#endif /* DISK_H */
