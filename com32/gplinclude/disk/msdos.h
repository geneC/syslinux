#ifndef _MSDOS_H_
#define _MSDOS_H_

#include <disk/geom.h>

int parse_partition_table(struct driveinfo *, void *, int *);

#endif /* _MSDOS_H_ */
