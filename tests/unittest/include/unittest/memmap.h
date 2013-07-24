#ifndef _UNITTEST_MEMMAP_H_
#define _UNITTEST_MEMMAP_H_

#include "syslinux/movebits.h"

#define array_sz(x)	(sizeof((x)) / sizeof((x)[0]))

struct test_memmap_entry {
    addr_t start;
    addr_t size;
    enum syslinux_memmap_types type;
};

extern struct syslinux_memmap *
test_build_memmap(struct test_memmap_entry *entries, size_t nr_entries);

#endif /* _UNITTEST_MEMMAP_H_ */
