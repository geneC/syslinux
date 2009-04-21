#ifndef _READ_H_
#define _READ_H_

#include <disk/geom.h>

void *read_mbr(int, int*);
void *dev_read(int, unsigned int, int, int*);
void *read_sectors(struct driveinfo*, const unsigned int,
		   const int, int *);
#endif /* _READ_H */
