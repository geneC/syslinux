#include "unittest/unittest.h"
#include "unittest/memmap.h"

#include "syslinux/bootrm.h"
#include <string.h>

/*
 * load_linux.c dependencies.
 */
#include "../../suffix_number.c"

static struct firmware __test_firmware;
struct firmware *firmware = &__test_firmware;

static struct syslinux_memmap *__test_mmap;
struct syslinux_memmap *syslinux_memory_map(void)
{
    return syslinux_dup_memmap(__test_mmap);
}

void syslinux_force_text_mode(void) { }

static char *__test_cmdline = "this is a test!!";
static bool __test_called_boot_rm = false;
static addr_t __test_cmdline_addr;

int syslinux_shuffle_boot_rm(struct syslinux_movelist *fraglist,
			     struct syslinux_memmap *memmap,
			     uint16_t bootflags,
			     struct syslinux_rm_regs *regs)
{
    struct syslinux_movelist *moves, *ml;
    int rv;

    __test_called_boot_rm = true;

    ml = fraglist;
    while (ml) {
	addr_t cmdline_addr, last_lowmem_addr;

	if (ml->src != __test_cmdline)
	    continue;

	last_lowmem_addr = __test_cmdline_addr;
	cmdline_addr = ml->dst;
	syslinux_assert_str(cmdline_addr == last_lowmem_addr,
			    "cmdline at 0x%x but expected 0x%x",
			    cmdline_addr, last_lowmem_addr);

	syslinux_assert_str(strlen(__test_cmdline) + 1 == ml->len,
			    "cmdline length %d, expected %d", ml->len,
			    strlen(__test_cmdline) + 1);
	break;
    }

    moves = NULL;
    rv = syslinux_compute_movelist(&moves, fraglist, memmap);
    syslinux_free_movelist(moves);

    syslinux_assert(!rv, "Failed to compute movelist");

    return -1;
}

#include "../load_linux.c"
#include "../zonelist.c"
#include "test-harness.c"

static void __test_setup_kernel(void *buf)
{
    struct linux_header *hdr;

    hdr = buf;
    memset(hdr, 0, sizeof(*hdr));

    /*
     * Setup the minimum required fields.
     */
    hdr->boot_flag = BOOT_MAGIC;
    hdr->setup_sects = 1;
    hdr->version = 0x0201;
}

static inline addr_t __test_calc_cmdline_addr(addr_t addr)
{
    size_t len = strlen(__test_cmdline) + 1;
    return (addr - len) & ~15;
}

#define KERNEL_BUF_SIZE		1024
static void *__test_setup(struct test_memmap_entry *entries,
			  size_t nr_entries,
			  addr_t last)
{
    struct syslinux_memmap *mmap;
    void *buf;

    mmap = test_build_mmap(entries, nr_entries);
    if (!mmap)
	goto bail;

    buf = malloc(KERNEL_BUF_SIZE);
    if (!buf)
	goto bail;

    __test_setup_kernel(buf);

    __test_mmap = mmap;
    __test_cmdline_addr = __test_calc_cmdline_addr(last);

    return buf;

bail:
    syslinux_free_memmap(mmap);
    return NULL;
}

static void __test_teardown(void *buf)
{
    free(buf);
    syslinux_free_memmap(__test_mmap);

    __test_called_boot_rm = false;
    __test_cmdline_addr = 0;
    __test_mmap = NULL;
}

/*
 * Make sure that we can relocate the cmdline to a free region of
 * memory.
 *
 * The below memory map is based on one from VMWare.
 */
static int test_cmdline_placement(void)
{
    struct syslinux_memmap *mmap;
    addr_t addr;
    void *buf;
    int rv;

    struct test_memmap_entry entries[] = {
	0x00000000, 0x00092800, SMT_FREE,
	0x00092800, 0x0000d800, SMT_RESERVED,
	0x000ca000, 0x00002000, SMT_RESERVED,
	0x000dc000, 0x00004000, SMT_RESERVED,
	0x000e4000, 0x00001c00, SMT_RESERVED,
	0x00100000, 0x3fdf0000, SMT_FREE,
    };

    buf = __test_setup(entries, array_sz(entries), 0x92800);
    if (!buf)
	return -1;

    rv = syslinux_boot_linux(buf, KERNEL_BUF_SIZE, NULL, NULL, __test_cmdline);

    syslinux_assert(__test_called_boot_rm,
		    "Failed to invoke syslinux_shuffle_boot_rm()");

    __test_teardown(buf);
    return 0;
}

/*
 * Ensure that the linux loader only uses SMT_TERMINAL regions as a last
 * resort.
 */
static int test_terminal_regions(void)
{
    addr_t addr;
    void *buf;
    int rv;

    struct test_memmap_entry entries[] = {
	0x000000, 0x090000, SMT_RESERVED,
	0x090000, 0x000420, SMT_TERMINAL,
	0x090420, 0x000400, SMT_FREE,
	0x090820, 0x000200, SMT_TERMINAL,
    };

    buf = __test_setup(entries, array_sz(entries), 0x090820);
    if (!buf)
	return -1;

    rv = syslinux_boot_linux(buf, KERNEL_BUF_SIZE, NULL, NULL, __test_cmdline);

    syslinux_assert(__test_called_boot_rm,
		    "Failed to invoke syslinux_shuffle_boot_rm()");

    __test_teardown(buf);
    return 0;
}

int main(int argc, char **argv)
{
    test_cmdline_placement();
    test_terminal_regions();

    return 0;
}
