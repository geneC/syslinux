#ifndef _WRITE_H_
#define _WRITE_H_

#include <disk/geom.h>

int write_sectors(const struct driveinfo*, const unsigned int,
		  const void *, const int, int *);
int write_verify_sector(struct driveinfo* drive_info,
			const unsigned int,
			const void *, int*);
int write_verify_sectors(struct driveinfo*,
			 const unsigned int,
			 const void *, const int, int *);
#endif
