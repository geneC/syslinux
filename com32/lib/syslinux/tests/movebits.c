#include "unittest/unittest.h"
#include "unittest/memmap.h"
#include <setjmp.h>

#include "../../../include/minmax.h"
#include "../zonelist.c"
#include "test-harness.c"

static int move_to_terminal_region(void)
{
    struct syslinux_memmap *mmap;
    addr_t dst, src;
    size_t len;
    int rv = -1;
    struct test_memmap_entry entries[] = {
	{ 0x00000, 0x90000, SMT_RESERVED },
	{ 0x90000, 0x10000, SMT_TERMINAL },
	{ 0xa0000, 0xf000, SMT_FREE },
	{ 0x100000, 0x3000, SMT_FREE }
    };

    mmap = test_build_mmap(entries, array_sz(entries));
    if (!mmap)
	goto bail;

    dst = 0x90000;
    src = 0x1fff000;
    len = 0xf000;

    rv = syslinux_memmap_find(mmap, &dst, len, false, 16,
			      0, (addr_t)-1, 0, (addr_t)-1);
    syslinux_assert(!rv, "Expected to find 0x%x to be SMT_TERMINAL", dst);

    rv = test_attempt_movelist(mmap, dst, src, len);
    syslinux_assert(!rv, "Expected to move 0x%x to 0x%x, len 0x%x", src, dst, len);

    rv = 0;

bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int move_to_overlapping_region(void)
{
    struct syslinux_memmap *mmap;
    addr_t dst, src;
    size_t len;
    int rv = -1;
    struct test_memmap_entry entries[] = {
	{ 0x00000, 0x90000, SMT_RESERVED },
	{ 0x90000, 0x10000, SMT_TERMINAL },
	{ 0xa0000, 0xf000, SMT_FREE },
	{ 0x100000, 0x3000, SMT_TERMINAL },
	{ 0x103000, 0x1000, SMT_FREE },
    };

    mmap = test_build_mmap(entries, array_sz(entries));
    if (!mmap)
	goto bail;

    rv = test_attempt_movelist(mmap, 0x90000, 0x300000, 0x10001);
    syslinux_assert(!rv, "Allocating across boundary region failed");

    rv = test_attempt_movelist(mmap, 0xa0000, 0x4000000, 0x10000);
    syslinux_assert(rv, "Move into undefined region succeeded");

    rv = test_attempt_movelist(mmap, 0x80000, 0x200000, 0x10000);
    syslinux_assert(rv, "Move across incompatible region boundary succeeded");

    rv = test_attempt_movelist(mmap, 0x100000, 0x4000000, 0x4001);
    syslinux_assert(rv, "Move past end of available regions succeeded");

    rv = 0;
bail:
    syslinux_free_memmap(mmap);
    return rv;
}

int main(int argc, char **argv)
{
    move_to_terminal_region();
    move_to_overlapping_region();

    return 0;
}
