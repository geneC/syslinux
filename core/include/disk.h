#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define SECTOR_SHIFT     9
#define SECTOR_SIZE      (1 << SECTOR_SHIFT)

typedef uint64_t sector_t;
typedef uint32_t block_t;

extern void read_sectors(char *, sector_t, int);
extern void getoneblk(char *, block_t, int);

#endif /* DISK_H */
