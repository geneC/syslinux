#include <core.h>
#include <fs.h>
#include <com32.h>
#include <syslinux/memscan.h>
#include <syslinux/firmware.h>
#include <syslinux/linux.h>
#include <sys/ansi.h>

#include "efi.h"
#include "fio.h"

char KernelName[FILENAME_MAX];
uint16_t PXERetry;
char copyright_str[] = "Copyright (C) 2011\n";
uint8_t SerialNotice = 1;
char syslinux_banner[] = "Syslinux 5.x (EFI)\n";
char CurrentDirName[FILENAME_MAX];
struct com32_sys_args __com32;

uint32_t _IdleTimer = 0;
uint16_t NoHalt = 0;
char __lowmem_heap[32];
uint32_t BIOS_timer_next;
uint32_t timer_irq;
uint8_t KbdMap[256];
uint16_t VGAFontSize = 16;
char aux_seg[256];
uint8_t UserFont = 0;

#undef kaboom
void kaboom(void)
{
}

void comboot_cleanup_api(void)
{
}

void printf_init(void)
{
}

void local_boot16(void)
{
}

void bios_timer_cleanup(void)
{
}

char trackbuf[4096];

void __cdecl core_farcall(uint32_t c, const com32sys_t *a, com32sys_t *b)
{
}

struct firmware *firmware = NULL;
void *__syslinux_adv_ptr;
size_t __syslinux_adv_size;
char core_xfer_buf[65536];
struct iso_boot_info {
	uint32_t pvd;               /* LBA of primary volume descriptor */
	uint32_t file;              /* LBA of boot file */
	uint32_t length;            /* Length of boot file */
	uint32_t csum;              /* Checksum of boot file */
	uint32_t reserved[10];      /* Currently unused */
} iso_boot_info;

struct ip_info {
	uint32_t ipv4;
	uint32_t myip;
	uint32_t serverip;
	uint32_t gateway;
	uint32_t netmask;
} IPInfo;

uint8_t DHCPMagic;
uint32_t RebootTime;

void pxenv(void)
{
}

uint16_t numIPAppends = 0;
char *IPAppends = NULL;
uint16_t BIOS_fbm = 1;
far_ptr_t InitStack;
uint16_t APIVer;
far_ptr_t PXEEntry;

void gpxe_unload(void)
{
}

void do_idle(void)
{
}

void pxe_int1a(void)
{
}

uint8_t KeepPXE;


volatile uint32_t __ms_timer = 0xdeadbeef;
volatile uint32_t __jiffies = 0;

static UINTN cursor_x, cursor_y;
static void efi_erase(const struct term_state *st,
		       int x0, int y0, int x1, int y1)
{
	cursor_x = cursor_y = 0;
}

static void efi_write_char(uint8_t ch, uint8_t attribute)
{
	SIMPLE_TEXT_OUTPUT_INTERFACE *out = ST->ConOut;
	uint16_t c[2];

	c[0] = ch;
	c[1] = '\0';
	uefi_call_wrapper(out->OutputString, 2, out, c);
}

static void efi_showcursor(uint16_t cursor)
{
	SIMPLE_TEXT_OUTPUT_INTERFACE *out = ST->ConOut;
	uefi_call_wrapper(out->SetCursorPosition, 3, out, cursor_x, cursor_y);
}

static void efi_set_cursor(int x, int y, bool visible)
{
	SIMPLE_TEXT_OUTPUT_INTERFACE *out = ST->ConOut;

	if (visible) {
		uefi_call_wrapper(out->SetCursorPosition, 3, out, x, y);
		cursor_x = x;
		cursor_y = y;
	} else
		uefi_call_wrapper(out->EnableCursor, 2, out, false);
}

static void efi_scroll_up(uint8_t cols, uint8_t rows, uint8_t attribute)
{
}


static void efi_get_mode(int *rows, int *cols)
{
	SIMPLE_TEXT_OUTPUT_INTERFACE *out = ST->ConOut;
	UINTN c, r;

	/* XXX: Assume we're at 80x25 for now (mode 0) */
	uefi_call_wrapper(out->QueryMode, 4, out, 0, &c, &r);
	*rows = r;
	*cols = c;
}

static void efi_set_mode(uint16_t mode)
{
}

static void efi_get_cursor(int *x, int *y)
{
	*x = cursor_x;
	*y = cursor_y;
}

struct output_ops efi_ops = {
	.erase = efi_erase,
	.write_char = efi_write_char,
	.showcursor = efi_showcursor,
	.set_cursor = efi_set_cursor,
	.scroll_up = efi_scroll_up,
	.get_mode = efi_get_mode,
	.set_mode = efi_set_mode,
	.get_cursor = efi_get_cursor,
};

char SubvolName[2];
static inline EFI_MEMORY_DESCRIPTOR *
get_memory_map(UINTN *nr_entries, UINTN *key, UINTN *desc_sz,
	       uint32_t *desc_ver)
{
	return LibMemoryMap(nr_entries, key, desc_sz, desc_ver);
}


int efi_scan_memory(scan_memory_callback_t callback, void *data)
{
	UINTN nr_entries, key, desc_sz;
	UINTN buf;
	UINT32 desc_ver;
	int rv = 0;
	int i;

	buf = (UINTN)get_memory_map(&nr_entries, &key, &desc_sz, &desc_ver);
	if (!buf)
		return -1;

	for (i = 0; i < nr_entries; buf += desc_sz, i++) {
		EFI_MEMORY_DESCRIPTOR *m;
		UINT64 region_sz;
		int valid;

		m = (EFI_MEMORY_DESCRIPTOR *)buf;
		region_sz = m->NumberOfPages * EFI_PAGE_SIZE;

		switch (m->Type) {
                case EfiConventionalMemory:
			valid = 1;
                        break;
		default:
			valid = 0;
			break;
		}

		rv = callback(data, m->PhysicalStart, region_sz, valid);
		if (rv)
			break;
	}

	FreePool((void *)buf);
	return rv;
}

extern uint16_t *bios_free_mem;
void efi_init(void)
{
	/* XXX timer */
	*bios_free_mem = 0;
	mem_init();
}

char efi_getchar(void)
{
	SIMPLE_INPUT_INTERFACE *in = ST->ConIn;
	EFI_INPUT_KEY key;
	EFI_STATUS status;
	char c;

	do {
		status = uefi_call_wrapper(in->ReadKeyStroke, 2, in, &key);
	} while (status == EFI_NOT_READY);

	c = (char)key.UnicodeChar;
	return c;
}

struct input_ops efi_iops = {
	.getchar = efi_getchar,
};

char *efi_get_config_file_name(void)
{
	return ConfigName;
}

bool efi_ipappend_strings(char **list, int *count)
{
	*count = numIPAppends;
	*list = (char *)IPAppends;
}

extern void efi_adv_init(void);
extern int efi_adv_write(void);

struct adv_ops efi_adv_ops = {
	.init = efi_adv_init,
	.write = efi_adv_write,
};

struct efi_info {
	uint32_t load_signature;
	uint32_t systab;
	uint32_t desc_size;
	uint32_t desc_version;
	uint32_t memmap;
	uint32_t memmap_size;
	uint32_t systab_hi;
	uint32_t memmap_hi;
};

#define E820MAX	128
#define E820_RAM	1
#define E820_RESERVED	2
#define E820_ACPI	3
#define E820_NVS	4
#define E820_UNUSABLE	5

struct e820_entry {
	uint64_t start;
	uint64_t len;
	uint32_t type;
} __packed;

struct boot_params {
	struct screen_info screen_info;
	uint8_t _pad[0x1c0 - sizeof(struct screen_info)];
	struct efi_info efi;
	uint8_t _pad2[8];
	uint8_t e820_entries;
	uint8_t _pad3[0x2d0 - 0x1e8 - sizeof(uint8_t)];
	struct e820_entry e820_map[E820MAX];
} __packed;

#define EFI_LOAD_SIG	"EL32"

struct dt_desc {
	uint16_t limit;
	uint64_t *base;
} __packed;

struct dt_desc gdt = { 0x800, 0 };
struct dt_desc idt = { 0, 0 };

static inline EFI_MEMORY_DESCRIPTOR *
get_mem_desc(addr_t memmap, UINTN desc_sz, int i)
{
	return (EFI_MEMORY_DESCRIPTOR *)(memmap + (i * desc_sz));
}

EFI_HANDLE image_handle;

static inline UINT64 round_up(UINT64 x, UINT64 y)
{
	return (((x - 1) | (y - 1)) + 1);
}

static inline UINT64 round_down(UINT64 x, UINT64 y)
{
	return (x & ~(y - 1));
}

static void find_addr(EFI_PHYSICAL_ADDRESS *first,
		      EFI_PHYSICAL_ADDRESS *last,
		      EFI_PHYSICAL_ADDRESS min,
		      EFI_PHYSICAL_ADDRESS max,
		      size_t size, size_t align)
{
	EFI_MEMORY_DESCRIPTOR *map;
	EFI_STATUS status;
	UINT32 desc_ver;
	UINTN nr_entries, key, desc_sz;
	UINT64 addr;
	int i;

	map = get_memory_map(&nr_entries, &key, &desc_sz, &desc_ver);
	if (!map)
		return;

	for (i = 0; i < nr_entries; i++) {
		EFI_MEMORY_DESCRIPTOR *m;
		EFI_PHYSICAL_ADDRESS best;
		UINT64 start, end;

		m = get_mem_desc((addr_t)map, desc_sz, i);
		if (m->Type != EfiConventionalMemory)
			continue;

		if (m->NumberOfPages < EFI_SIZE_TO_PAGES(size))
			continue;

		start = m->PhysicalStart;
		end = m->PhysicalStart + (m->NumberOfPages << EFI_PAGE_SHIFT);
		if (first) {
			if (end < min)
				continue;

			/* What's the best address? */
			if (start < min && min < end)
				best = min;
			else
				best = m->PhysicalStart;

			start = round_up(best, align);
			if (start > max)
				continue;

			/* Have we run out of space in this region? */
			if (end < start || (start + size) > end)
				continue;

			if (start < *first)
				*first = start;
		}

		if (last) {
			if (start > max)
				continue;

			/* What's the best address? */
			if (start < max && max < end)
				best = max - size;
			else
				best = end - size;

			start = round_down(best, align);
			if (start < min || start < m->PhysicalStart)
				continue;

			if (start > *last)
				*last = start;
		}
	}

	FreePool(map);
}

static EFI_STATUS allocate_addr(EFI_PHYSICAL_ADDRESS *addr, size_t size)
{
	UINTN npages = EFI_SIZE_TO_PAGES(size);

	return uefi_call_wrapper(BS->AllocatePages, 4,
				   AllocateAddress,
				   EfiLoaderData, npages,
				   addr);
}

static void free_addr(EFI_PHYSICAL_ADDRESS addr, size_t size)
{
	UINTN npages = EFI_SIZE_TO_PAGES(size);

	uefi_call_wrapper(BS->FreePages, 2, addr, npages);
}

int efi_boot_linux(void *kernel_buf, size_t kernel_size,
		   struct initramfs *initramfs, char *cmdline)
{
	EFI_MEMORY_DESCRIPTOR *map;
	struct linux_header *hdr;
	struct boot_params *bp;
	struct screen_info *si;
	struct e820_entry *e820buf, *e;
	EFI_STATUS status;
	EFI_PHYSICAL_ADDRESS first, last;
	UINTN nr_entries, key, desc_sz;
	UINT32 desc_ver;
	uint32_t e820_type;
	addr_t irf_size;
	int i;

	hdr = (struct linux_header *)kernel_buf;
	bp = (struct boot_params *)hdr;

	/*
	 * We require a relocatable kernel because we have no control
	 * over free memory in the memory map.
	 */
	if (hdr->version < 0x20a || !hdr->relocatable_kernel) {
		printf("bzImage version unsupported\n");
		goto bail;
	}

	hdr->type_of_loader = 0x30;	/* SYSLINUX unknown module */
	hdr->cmd_line_ptr = cmdline;

	si = &bp->screen_info;
	memset(si, 0, sizeof(*si));
	setup_screen(si);

	gdt.base = (uint16_t *)malloc(gdt.limit);
	memset(gdt.base, 0x0, gdt.limit);

	first = -1ULL;
	find_addr(&first, NULL, 0x1000, -1ULL, kernel_size,
		  hdr->kernel_alignment);
	if (first != -1ULL)
		status = allocate_addr(&first, kernel_size);

	if (first == -1ULL || status != EFI_SUCCESS) {
		printf("Failed to allocate memory for kernel\n");
		goto bail;
	}

	hdr->code32_start = (uint32_t)first;

	/* Skip the setup headers and copy the code */
	kernel_buf += (hdr->setup_sects + 1) * 512;
	memcpy(hdr->code32_start, kernel_buf, kernel_size);

	/*
	 * Figure out the size of the initramfs, and where to put it.
	 * We should put it at the highest possible address which is
	 * <= hdr->initrd_addr_max, which fits the entire initramfs.
	 */
	irf_size = initramfs_size(initramfs);	/* Handles initramfs == NULL */
	if (irf_size) {
		struct initramfs *ip;
		addr_t next_addr, len, pad;

		last = 0;
		find_addr(NULL, &last, 0x1000, hdr->initrd_addr_max,
			  irf_size, INITRAMFS_MAX_ALIGN);
		if (last)
			status = allocate_addr(&last, irf_size);

		if (!last || status != EFI_SUCCESS) {
			printf("Failed to allocate initramfs memory\n");
			goto free_kernel;
		}

		hdr->ramdisk_image = (uint32_t)last;
		hdr->ramdisk_size = irf_size;

		/* Copy initramfs into allocated memory */
		for (ip = initramfs->next; ip->len; ip = ip->next) {
			len = ip->len;
			next_addr = last + len;

			/*
			 * If this isn't the last entry, extend the
			 * zero-pad region to enforce the alignment of
			 * the next chunk.
			 */
			if (ip->next->len) {
				pad = -next_addr & (ip->next->align - 1);
				len += pad;
				next_addr += pad;
			}

			if (ip->data_len)
				memcpy(last, ip->data, ip->data_len);

			if (len > ip->data_len)
				memset(last + ip->data_len, 0,
				       len - ip->data_len);

			last = next_addr;
		}
	}

	/* Build efi memory map */
	map = get_memory_map(&nr_entries, &key, &desc_sz, &desc_ver);
	if (!map)
		goto free_irf;

	bp->efi.memmap = map;
	bp->efi.memmap_size = nr_entries * desc_sz;

	bp->efi.systab = ST;
	bp->efi.desc_size = desc_sz;
	bp->efi.desc_version = desc_ver;

	/*
	 * Even though 'memmap' contains the memory map we provided
	 * previously in efi_scan_memory(), we should recalculate the
	 * e820 map because it will most likely have changed in the
	 * interim.
	 */
	e = e820buf = bp->e820_map;
	for (i = 0; i < nr_entries && i < E820MAX; i++) {
		struct e820_entry *prev = NULL;

		if (e > e820buf)
			prev = e - 1;

		map = get_mem_desc(bp->efi.memmap, desc_sz, i);
		e->start = map->PhysicalStart;
		e->len = map->NumberOfPages << EFI_PAGE_SHIFT;

		switch (map->Type) {
		case EfiReservedMemoryType:
                case EfiRuntimeServicesCode:
                case EfiRuntimeServicesData:
                case EfiMemoryMappedIO:
                case EfiMemoryMappedIOPortSpace:
                case EfiPalCode:
                        e820_type = E820_RESERVED;
                        break;

                case EfiUnusableMemory:
                        e820_type = E820_UNUSABLE;
                        break;

                case EfiACPIReclaimMemory:
                        e820_type = E820_ACPI;
                        break;

                case EfiLoaderCode:
                case EfiLoaderData:
                case EfiBootServicesCode:
                case EfiBootServicesData:
                case EfiConventionalMemory:
			e820_type = E820_RAM;
			break;

		case EfiACPIMemoryNVS:
			e820_type = E820_NVS;
			break;
		default:
			continue;
		}

		e->type = e820_type;

		/* Check for adjacent entries we can merge. */
		if (prev && (prev->start + prev->len) == e->start &&
		    prev->type == e->type)
			prev->len += e->len;
		else
			e++;
	}

	bp->e820_entries = e - e820buf;

	status = uefi_call_wrapper(BS->ExitBootServices, 2, image_handle, key);
	if (status != EFI_SUCCESS) {
		printf("Failed to exit boot services: 0x%016lx\n", status);
		goto free_map;
	}

	memcpy(&bp->efi.load_signature, EFI_LOAD_SIG, sizeof(uint32_t));

	/*
         * 4Gb - (0x100000*0x1000 = 4Gb)
         * base address=0
         * code read/exec
         * granularity=4096, 386 (+5th nibble of limit)
         */
        gdt.base[2] = 0x00cf9a000000ffff;

        /*
         * 4Gb - (0x100000*0x1000 = 4Gb)
         * base address=0
         * data read/write
         * granularity=4096, 386 (+5th nibble of limit)
         */
        gdt.base[3] = 0x00cf92000000ffff;

        /* Task segment value */
        gdt.base[4] = 0x0080890000000000;

	asm volatile ("lidt %0" :: "m" (idt));
	asm volatile ("lgdt %0" :: "m" (gdt));

	asm volatile ("cli              \n"
                      "movl %0, %%esi   \n"
                      "movl %1, %%ecx   \n"
                      "jmp *%%ecx       \n"
                      :: "m" (bp), "m" (hdr->code32_start));
	/* NOTREACHED */

free_map:
	FreePool(map);
free_irf:
	if (irf_size)
		free_addr(last, irf_size);
free_kernel:
	free_addr(first, kernel_size);
bail:
	return -1;
}

extern struct disk *efi_disk_init(com32sys_t *);
extern void serialcfg(uint16_t *, uint16_t *, uint16_t *);

struct firmware efi_fw = {
	.init = efi_init,
	.scan_memory = efi_scan_memory,
	.disk_init = efi_disk_init,
	.o_ops = &efi_ops,
	.i_ops = &efi_iops,
	.get_config_file_name = efi_get_config_file_name,
	.get_serial_console_info = serialcfg,
	.ipappend_strings = efi_ipappend_strings,
	.adv_ops = &efi_adv_ops,
	.boot_linux = efi_boot_linux,
};

static inline void syslinux_register_efi(void)
{
	firmware = &efi_fw;
}

extern void init(void);
extern const struct fs_ops vfat_fs_ops;

char free_high_memory[4096];

extern char __bss_start[];
extern char __bss_end[];
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table)
{
	EFI_LOADED_IMAGE *info;
	EFI_STATUS status = EFI_SUCCESS;
	struct fs_ops *ops[] = { &vfat_fs_ops, NULL };
	unsigned long len = (unsigned long)__bss_end - (unsigned long)__bss_start;

	memset(__bss_start, 0, len);
	InitializeLib(image, table);

	image_handle = image;
	syslinux_register_efi();
	init();

	status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
				   &LoadedImageProtocol, (void **)&info);
	if (status != EFI_SUCCESS) {
		Print(L"Failed to lookup LoadedImageProtocol\n");
		goto out;
	}

	/* Use device handle to set up the volume root to proceed with ADV init */
	if (EFI_ERROR(efi_set_volroot(info->DeviceHandle))) {
		Print(L"Failed to locate root device to prep for file operations & ADV initialization\n");
		goto out;
	}

	/* TODO: once all errors are captured in efi_errno, bail out if necessary */

	/* XXX figure out what file system we're on */
	fs_init(ops, info->DeviceHandle);
	load_env32();

out:
	return status;
}
