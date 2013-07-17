#ifndef _TEST_H_
#define _TEST_H_

#include "unittest.h"
#include "syslinux/movebits.h"

#define array_sz(x)	(sizeof((x)) / sizeof((x)[0]))

struct mmap_entry {
    addr_t start;
    addr_t size;
    enum syslinux_memmap_types type;
};

extern struct syslinux_memmap *build_mmap(struct mmap_entry *entries,
					  size_t nr_entries);
#endif /* _TEST_H_ */
