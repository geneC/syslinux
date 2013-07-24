#include "unittest/unittest.h"
#include "unittest/memmap.h"

/*
 * Fake data objects.
 *
 * These are the dependencies required by mem_init().
 */
struct com32_sys_args {
     unsigned long cs_memsize;
} __com32 = {
     .cs_memsize = 4
};
char __lowmem_heap[32];
char free_high_memory[32];

#include "../init.c"

void __inject_free_block(struct free_arena_header *ah)
{
}

static unsigned long free_start = (unsigned long)free_high_memory;

static inline bool free_list_empty(void)
{
    if (__com32.cs_memsize != free_start)
	return false;

    return true;
}

static struct test_memmap_entry *__test_entries;
static size_t __test_nr_entries;

int syslinux_scan_memory(scan_memory_callback_t callback, void *data)
{
    struct test_memmap_entry *e;
    int i;

    for (i = 0; i < __test_nr_entries; i++) {
	e = &__test_entries[i];
	callback(data, e->start, e->size, e->type);
    }

    return 0;
}

void __setup(struct test_memmap_entry *entries, size_t nr_entries)
{
    uint16_t __fake_free_mem = 64;

    bios_free_mem = &__fake_free_mem;

    __test_entries = entries;
    __test_nr_entries = nr_entries;
}

/*
 * scan_highmem_area() will prepend a free arena header if the size of
 * the region is larger than the following expression. Using this small
 * size allows us to test the interface safely without worrying about
 * scan_highmem_area() writing data to random parts of our address
 * space.
 */
#define safe_entry_sz	((2 * sizeof(struct arena_header)) - 1)

/*
 * Can we add SMT_RESERVED regions to the free list?
 */
static int test_mem_init_reserved(void)
{
    struct test_memmap_entry entries[] = {
	0x2000,   safe_entry_sz, SMT_RESERVED,
	0x100000, safe_entry_sz, SMT_RESERVED,
	0x2fffff, safe_entry_sz, SMT_RESERVED,
	0x400000, safe_entry_sz, SMT_RESERVED,
    };

    __setup(entries, array_sz(entries));

    mem_init();
    syslinux_assert_str(free_list_empty(),
			"Added SMT_RESERVED regions to free list");
    return 0;
}

/*
 * Can we add regions outside of the valid address range?
 */
static int test_mem_limits(void)
{
    struct test_memmap_entry entries[] = {
	0x00000000,   safe_entry_sz, SMT_FREE,
	0x000fffff,   safe_entry_sz, SMT_FREE,
	E820_MEM_MAX + 1, safe_entry_sz, SMT_FREE,
    };

    __setup(entries, array_sz(entries));

    mem_init();
    syslinux_assert_str(free_list_empty(),
			"Added regions outside of valid range to free list");

    return 0;
}


int main(int argc, char **argv)
{
    test_mem_init_reserved();
    test_mem_limits();

    return 0;
}
