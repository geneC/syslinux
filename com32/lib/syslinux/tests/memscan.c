#include "test.h"

#include "../memscan.c"

struct memmap {
    addr_t start;
    size_t size;
    enum syslinux_memmap_types type;
    bool visited;
};

static struct memmap memmap[] = {
    { 0x00000, 0x2000, SMT_FREE, false },
    { 0x400000, 0x1000, SMT_TERMINAL, false},
};

#define MEMMAP_SIZE (sizeof(memmap) / sizeof(memmap[0]))

/*
 * Our dummy memory scanner. This is analogous to bios_scan_memory() or
 * efi_scan_memory(), etc.
 */
static int test_scan_memory(scan_memory_callback_t callback, void *data)
{
    int i, rv;

    for (i = 0; i < MEMMAP_SIZE; i++)
	rv = callback(data, memmap[i].start, memmap[i].size, memmap[i].type);

    return 0;
}

static int callback(void *data, addr_t start, addr_t size,
		    enum syslinux_memmap_types type)
{
    int i;

    for (i = 0; i < MEMMAP_SIZE; i++) {
	if (memmap[i].start == start && memmap[i].size == size) {
	    memmap[i].visited = true;
	    break;
	}
    }

    return 0;
}

static int verify_visited_all_memmap_entries(void)
{
    int i;

    syslinux_memscan_new(test_scan_memory);
    syslinux_scan_memory(callback, NULL);

    for (i = 0; i < MEMMAP_SIZE; i++) {
	addr_t start = memmap[i].start;
	bool visited_entry = memmap[i].visited;

	syslinux_assert(visited_entry, "Didn't pass entry %d to callback", i);
    }

    return 0;
}

static int verify_invoked_all_callbacks(void)
{
    syslinux_scan_memory(callback, NULL);
}

int main(int argc, char **argv)
{
    verify_visited_all_memmap_entries();

    return 0;
}
