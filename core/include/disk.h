#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define SECTOR_SHIFT     9
#define SECTOR_SIZE      (1 << SECTOR_SHIFT)

typedef uint64_t sector_t;
typedef uint64_t block_t;


/*
 * struct disk: contains the information about a specific disk and also
 * contains the I/O function.
 */
struct disk {
    uint8_t  disk_number;  /* in BIOS style */
    uint8_t  type;         /* CHS or EDD */
    uint16_t sector_size;  /* gener512B or 2048B */
    uint8_t  sector_shift;
    
    uint8_t h, s;          /* CHS geometry */
    uint8_t pad;

    sector_t part_start;   /* the start address of this partition(in sectors) */
    
    int (*rdwr_sectors)(struct disk *, void *, sector_t, size_t, bool);
};

extern void read_sectors(char *, sector_t, int);
extern void getoneblk(char *, block_t, int);

/* diskio.c */
struct disk *disk_init(uint8_t, bool, sector_t, uint16_t, uint16_t);

#endif /* DISK_H */
