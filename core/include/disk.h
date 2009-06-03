#ifndef DISK_H
#define DISK_H

#define SECTOR_SHIFT     9
#define SECTOR_SIZE      (1 << SECTOR_SHIFT)

typedef unsigned int sector_t;
typedef unsigned int block_t;

extern void read_sectors(char *, sector_t, int);
extern void getoneblk(char *, block_t, int);

#endif /* DISK_H */
