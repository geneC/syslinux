#include <core.h>
#include <fs.h>
#include <com32.h>
#include <syslinux/memscan.h>
#include <syslinux/firmware.h>
#include <sys/ansi.h>

#include "efi.h"

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

void *__syslinux_adv_ptr; /* definitely needs to die: is in ldlinux now */
size_t __syslinux_adv_size; /* definitely needs to die: is in ldlinux now */
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

	syslinux_register_efi();
	init();

	status = uefi_call_wrapper(BS->HandleProtocol, 3, image,
				   &LoadedImageProtocol, (void **)&info);
	if (status != EFI_SUCCESS) {
		printf("Failed to lookup LoadedImageProtocol\n");
		goto out;
	}

	/* XXX figure out what file system we're on */
	fs_init(ops, info->DeviceHandle);
	load_env32();

out:
	return status;
}
