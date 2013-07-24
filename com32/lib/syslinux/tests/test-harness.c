#include "../addlist.c"
#include "../freelist.c"
#include "../movebits.c"

struct syslinux_memmap *test_build_mmap(struct test_memmap_entry *entries,
					size_t nr_entries)
{
    struct syslinux_memmap *mmap;
    int i;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    for (i = 0; i < nr_entries; i++) {
	enum syslinux_memmap_types type = entries[i].type;
	addr_t start = entries[i].start;
	addr_t size = entries[i].size;

	if (syslinux_add_memmap(&mmap, start, size, type))
	    goto bail;
    }

    return mmap;

bail:
    syslinux_free_memmap(mmap);
    return NULL;
}

int test_attempt_movelist(struct syslinux_memmap *mmap, addr_t dst,
			  addr_t src, size_t len)
{
    struct syslinux_movelist *frags = NULL;
    struct syslinux_movelist *moves = NULL;
    int rv;

    rv = syslinux_add_movelist(&frags, dst, src, len);
    if (rv)
	goto bail;

    rv = syslinux_compute_movelist(&moves, frags, mmap);

bail:
    syslinux_free_movelist(frags);
    syslinux_free_movelist(moves);
    return rv;
}
