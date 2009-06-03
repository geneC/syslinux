#ifndef DISK_H
#define DISK_H

#define SECTOR_SHIFT     9
#define SECTOR_SIZE      (1 << SECTOR_SHIFT)


extern void read_sectors(char *, int, int);
extern void get_cache_block(com32sys_t *);

#endif /* DISK_H */
