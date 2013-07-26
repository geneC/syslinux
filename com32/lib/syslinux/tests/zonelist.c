/*
 * Unit test.
 *
 * We make heavy use of assertions to ensure that our expectations are
 * met regarding the Syslinux library interfaces. If an assert fails we
 * keep running the test if possible to try and save as much information
 * regarding the failures.
 *
 * A unit test function should return an error if it failed to setup the
 * infrastructure, e.g. malloc() fails. If an assertion fires, that is
 * not mean the test infrastructure failed, merely that the test didn't
 * pass.
 *
 * To work around any include path issues due to the unit tests being
 * run on the development host we must include all headers with absolute
 * paths.
 */
#include "unittest/unittest.h"
#include "unittest/memmap.h"
#include "../zonelist.c"
#include "test-harness.c"

static int refuse_to_alloc_reserved_region(void)
{
    struct syslinux_memmap *mmap;
    const char *cmdline;
    size_t kernel_size;
    bool relocatable = false;
    size_t alignment = 1;
    addr_t base;
    int rv = -1;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    if (syslinux_add_memmap(&mmap, 0x90000, 0x10000, SMT_RESERVED))
	goto bail;

    base = 0x90000;
    rv = syslinux_memmap_find(mmap, &base, 0x1000, relocatable, alignment,
			      base, base, 0, 640 * 1024);
    syslinux_assert(rv, "Allocated reserved region 0x%x", base);

    rv = 0;

bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int refuse_to_alloc_above_max_address(void)
{
    struct syslinux_memmap *mmap;
    addr_t base = 0x100000;
    size_t size = 0x1000;
    int rv = -1;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    if (syslinux_add_memmap(&mmap, base, size, SMT_FREE))
	goto bail;

    rv = syslinux_memmap_find(mmap, &base, size, false, 16,
			      base, base, 0, 640 * 1024);
    syslinux_assert(!rv, "Failed to find a free region");

    syslinux_assert_str(!(base < 0x100000), "Region below min address");
    syslinux_assert_str(!(base > 0x100000), "Region above max address");

    rv = 0;
bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int alloc_region_with_zero_size(void)
{
    int rv;

    rv = syslinux_memmap_find(NULL, 0, 0, false, 0, 0, 0, 0, 0); 
    syslinux_assert(!rv, "Should be able to allocate a zero-size region");

    return 0;
}

static int refuse_to_relocate_region(void)
{
    struct syslinux_memmap *mmap;
    addr_t free_base;
    size_t free_size;
    int rv = -1;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    free_base = 0x20000;
    free_size = 0x1000000;
    if (syslinux_add_memmap(&mmap, free_base, free_size, SMT_FREE))
	goto bail;

    free_base = 0x10000;
    free_size = 0x7000;
    rv = syslinux_memmap_find(mmap, &free_base, free_size, false, 1,
			      0, (addr_t)-1, (addr_t)0, (addr_t)-1);
    syslinux_assert(rv, "Relocated region from 0x10000 to 0x%x", free_base);

bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int only_relocate_upwards(void)
{
    struct syslinux_memmap *mmap;
    addr_t base;
    int rv = -1;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    if (syslinux_add_memmap(&mmap, 0x00010, 0x1000, SMT_FREE))
	goto bail;

    base = 0x10000;
    rv = syslinux_memmap_find(mmap, &base, 16, true, 1, 0, (addr_t)-1,
			      0x2000, (addr_t)-1);

    syslinux_assert(rv, "Should not have found any entry in memmap");
    syslinux_assert_str(base >= 0x10000,
			"Relocated in wrong direction 0x%x", base);

    rv = 0;
bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int alloc_in_pxe_region(void)
{
    struct syslinux_memmap *mmap;
    addr_t base;
    int rv = -1;

    mmap = syslinux_init_memmap();
    if (!mmap)
	goto bail;

    /* Construct a memmap with a gap where the PXE region usually is */
    if (syslinux_add_memmap(&mmap, 0x00000, 0x8f000, SMT_FREE))
	goto bail;

    if (syslinux_add_memmap(&mmap, 0x100000, 0xf000, SMT_FREE))
	goto bail;

    base = 0x90000;
    rv = syslinux_memmap_find(mmap, &base, 0x1000, false, 16,
			      0, (addr_t)-1, 0, (addr_t)-1);

    syslinux_assert(rv, "Shouldn't have allocated none existent region");

bail:
    syslinux_free_memmap(mmap);
    return rv;
}

static int demote_free_region_to_terminal(void)
{
    enum syslinux_memmap_types type;
    struct syslinux_memmap *mmap;
    int rv = -1;
    struct test_memmap_entry entries[] = {
	{ 0x100000, 0x300000, SMT_TERMINAL },
	{ 0x400000, 0x300000, SMT_FREE },
	{ 0x700000, 0x20000, SMT_FREE },
	{ 0x720000, 0x20000, SMT_TERMINAL },
    };

    mmap = test_build_mmap(entries, array_sz(entries));
    if (!mmap)
	goto bail;

    type = syslinux_memmap_type(mmap, 0x100000, 0x500000);
    syslinux_assert_str(type == SMT_TERMINAL,
	"Expected SMT_TERMINAL + SMT_FREE region to cause type demotion");

    type = syslinux_memmap_type(mmap, 0x700000, 0x40000);
    syslinux_assert_str(type == SMT_TERMINAL,
	"Expected SMT_FREE + SMT_TERMINAL region to cause type demotion");

    type = syslinux_memmap_type(mmap, 0x100000, 0x640000);
    syslinux_assert_str(type == SMT_TERMINAL,
	"Expected multiple SMT_{FREE,TERMINAL} regions to cause type demotion");

    rv = 0;

bail:
    syslinux_free_memmap(mmap);
    return rv;
}

/*
 * Find the highest address given a set of boundary conditions.
 */
static int test_find_highest(void)
{
    struct syslinux_memmap *mmap;
    addr_t start;
    int rv = -1;
    struct test_memmap_entry entries[] = {
	0x000000, 0x000000, SMT_FREE,
	0x090000, 0x002000, SMT_FREE,
	0x092000, 0x100000, SMT_RESERVED,
	0x192001, 0x000021, SMT_TERMINAL,
    };

    mmap = test_build_mmap(entries, array_sz(entries));
    if (!mmap)
	goto bail;

    start = 0x90000;
    rv = syslinux_memmap_highest(mmap, SMT_FREE, &start, 0x20, 0x192000, 1);

    syslinux_assert(!rv, "Failed to find highest SMT_FREE address");
    syslinux_assert_str(start == 0x91fe0,
			"0x%x incorrect highest address", start);

    start = 0x40000;
    rv = syslinux_memmap_highest(mmap, SMT_FREE, &start, 0x20, 0x91480, 1);

    syslinux_assert(!rv, "Failed to find highest SMT_FREE address");
    syslinux_assert_str(start == 0x91460,
			"0x%x incorrect highest address", start);


    start = 0x90023;
    rv = syslinux_memmap_highest(mmap, SMT_FREE, &start, 0x20, 0x90057, 0x10);
    syslinux_assert_str(start == 0x90030,
			"0x%x incorrectly aligned highest address", start);

    start = 0x00000;
    rv = syslinux_memmap_highest(mmap, SMT_TERMINAL, &start,
				 0x7, 0x192300, 0x10);
    syslinux_assert_str(start == 0x192010,
			"0x%x incorrectly aligned SMT_TERMINAL address", start);

    start = 0x192001;
    rv = syslinux_memmap_highest(mmap, SMT_RESERVED, &start,
				 0x400, (addr_t)-1, 1);
    syslinux_assert_str(rv && start == 0x192001,
			"Unexpectedly succeeded finding invalid region: 0x%x",
			start);

    start = 0x191fff;
    rv = syslinux_memmap_highest(mmap, SMT_RESERVED, &start,
				 0x800, (addr_t)-1, 1);
    syslinux_assert_str(rv && start == 0x191fff,
			"Unexpectedly succeeded finding invalid region: 0x%x",
			start);
    rv = 0;
bail:
    syslinux_free_memmap(mmap);
    return rv;
}

int main(int argc, char **argv)
{
    refuse_to_alloc_reserved_region();
    refuse_to_alloc_above_max_address();

    alloc_region_with_zero_size();

    refuse_to_relocate_region();
    only_relocate_upwards();

    alloc_in_pxe_region();
    demote_free_region_to_terminal();

    test_find_highest();

    return 0;
}
