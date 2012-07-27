#include <sys/ansi.h>
#include <sys/io.h>
#include <fs.h>
#include <bios.h>
#include <com32.h>
#include <graphics.h>
#include <syslinux/memscan.h>
#include <syslinux/firmware.h>

struct firmware *firmware = NULL;

extern struct ansi_ops bios_ansi_ops;

#define BIOS_CURXY ((struct curxy *)0x450)	/* Array for each page */
#define BIOS_ROWS (*(uint8_t *)0x484)	/* Minus one; if zero use 24 (= 25 lines) */
#define BIOS_COLS (*(uint16_t *)0x44A)
#define BIOS_PAGE (*(uint8_t *)0x462)

static void bios_set_mode(uint16_t mode)
{
    syslinux_force_text_mode();
}

static void bios_get_mode(int *cols, int *rows)
{
    *rows = BIOS_ROWS ? BIOS_ROWS + 1 : 25;
    *cols = BIOS_COLS;
}

static uint16_t cursor_type;	/* Saved cursor pattern */

static void bios_get_cursor(int *x, int *y)
{
    com32sys_t ireg, oreg;

    memset(&ireg, 0, sizeof(ireg));

    ireg.eax.b[1] = 0x03;
    ireg.ebx.b[1] = BIOS_PAGE;
    __intcall(0x10, &ireg, &oreg);
    cursor_type = oreg.ecx.w[0];
    *x = oreg.edx.b[0];
    *y = oreg.edx.b[1];
}

static void bios_erase(int x0, int y0, int x1, int y1, uint8_t attribute)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0600;	/* Clear window */
    ireg.ebx.b[1] = attribute;
    ireg.ecx.b[0] = x0;
    ireg.ecx.b[1] = y0;
    ireg.edx.b[0] = x1;
    ireg.edx.b[1] = y1;
    __intcall(0x10, &ireg, NULL);
}

static void bios_showcursor(const struct term_state *st)
{
    static com32sys_t ireg;
    uint16_t cursor = st->cursor ? cursor_type : 0x2020;

    ireg.eax.b[1] = 0x01;
    ireg.ecx.w[0] = cursor;
    __intcall(0x10, &ireg, NULL);
}

static void bios_set_cursor(int x, int y, bool visible)
{
    const int page = BIOS_PAGE;
    struct curxy xy = BIOS_CURXY[page];
    static com32sys_t ireg;

    (void)visible;

    if (xy.x != x || xy.y != y) {
	ireg.eax.b[1] = 0x02;
	ireg.ebx.b[1] = page;
	ireg.edx.b[1] = y;
	ireg.edx.b[0] = x;
	__intcall(0x10, &ireg, NULL);
    }
}

static void bios_write_char(uint8_t ch, uint8_t attribute)
{
    static com32sys_t ireg;

    ireg.eax.b[1] = 0x09;
    ireg.eax.b[0] = ch;
    ireg.ebx.b[1] = BIOS_PAGE;
    ireg.ebx.b[0] = attribute;
    ireg.ecx.w[0] = 1;
    __intcall(0x10, &ireg, NULL);
}

static void bios_scroll_up(uint8_t cols, uint8_t rows, uint8_t attribute)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0601;
    ireg.ebx.b[1] = attribute;
    ireg.ecx.w[0] = 0;
    ireg.edx.b[1] = rows;
    ireg.edx.b[0] = cols;
    __intcall(0x10, &ireg, NULL);	/* Scroll */
}

static void bios_beep(void)
{
    static com32sys_t ireg;

    ireg.eax.w[0] = 0x0e07;
    ireg.ebx.b[1] = BIOS_PAGE;
    __intcall(0x10, &ireg, NULL);
}

struct output_ops bios_output_ops = {
	.erase = bios_erase,
	.write_char = bios_write_char,
	.showcursor = bios_showcursor,
	.set_cursor = bios_set_cursor,
	.scroll_up = bios_scroll_up,
	.beep = bios_beep,
	.get_mode = bios_get_mode,
	.set_mode = bios_set_mode,
	.get_cursor = bios_get_cursor,
};

extern char bios_getchar(char *);
extern int bios_pollchar(void);

struct input_ops bios_input_ops = {
	.getchar = bios_getchar,
	.pollchar = bios_pollchar,
};

static const char *syslinux_ipappend_string_list[32];
bool bios_ipappend_strings(char **list, int *count)
{
    static com32sys_t reg;
    int i;

    reg.eax.w[0] = 0x000f;
    __intcall(0x22, &reg, &reg);

    if (reg.eflags.l & EFLAGS_CF)
	return false;

    for (i = 0; i < reg.ecx.w[0]; i++) {
	syslinux_ipappend_string_list[i] =
	    MK_PTR(reg.es,
		   *(uint16_t *) MK_PTR(reg.es, reg.ebx.w[0] + i * 2));
    }

    *list = syslinux_ipappend_string_list;
    *count = reg.ecx.w[0];

    return true;
}

static void bios_get_serial_console_info(uint16_t *iobase, uint16_t *divisor,
					 uint16_t *flowctl)
{
    *iobase = SerialPort;
    *divisor = BaudDivisor;

    *flowctl = FlowOutput | FlowInput | (FlowIgnore << 4);

    if (!DisplayCon)
	*flowctl |= (0x80 << 8);
}

void *__syslinux_adv_ptr;
size_t __syslinux_adv_size;

void bios_adv_init(void)
{
    static com32sys_t reg;

    reg.eax.w[0] = 0x0025;
    __intcall(0x22, &reg, &reg);

    reg.eax.w[0] = 0x001c;
    __intcall(0x22, &reg, &reg);
    __syslinux_adv_ptr = MK_PTR(reg.es, reg.ebx.w[0]);
    __syslinux_adv_size = reg.ecx.w[0];
}

int bios_adv_write(void)
{
    static com32sys_t reg;

    reg.eax.w[0] = 0x001d;
    __intcall(0x22, &reg, &reg);
    return (reg.eflags.l & EFLAGS_CF) ? -1 : 0;
}

struct adv_ops bios_adv_ops = {
	.init = bios_adv_init,
	.write = bios_adv_write,
};

static uint32_t min_lowmem_heap = 65536;
extern char __lowmem_heap[];
uint8_t KbdFlags;		/* Check for keyboard escapes */

static inline void check_escapes(void)
{
	com32sys_t ireg, oreg;

	ireg.eax.b[1] = 0x02;	/* Check keyboard flags */
	__intcall(0x16, &ireg, &oreg);

	KbdFlags = oreg.eax.b[0];

	/* Ctrl->skip 386 check */
	if (oreg.eax.b[0] & 0x04) {
		/*
		 * Now check that there is sufficient low (DOS) memory
		 *
		 * NOTE: Linux doesn't use all of real_mode_seg, but we use
		 * the same segment for COMBOOT images, which can use all 64K.
		 */
		uint16_t mem;

		__intcall(0x12, &ireg, &oreg);

		mem = ((uint32_t)__lowmem_heap) + min_lowmem_heap + 1023;
		mem = mem >> 10;

		if (mem < oreg.eax.w[0]) {
			char buf[256];

			snprintf(buf, sizeof(buf),
				 "It appears your computer has only "
				 "%dK of low (\"DOS\") RAM.\n"
				 "This version of Syslinux needs "
				 "%dK to boot.  "
				 "If you get this\nmessage in error, "
				 "hold down the Ctrl key while booting, "
				 "and I\nwill take your word for it.\n",
				 oreg.eax.w[0], mem);
			writestr(buf);
			kaboom();
		}
	}
}

extern uint32_t BIOS_timer_next;
extern uint32_t timer_irq;
static inline void bios_timer_init(void)
{
	unsigned long next;
	uint32_t *hook = (uint32_t *)BIOS_timer_hook;

	next = *hook;
	BIOS_timer_next = next;
	*hook = (uint32_t)&timer_irq;
}

extern uint8_t bios_free_mem;
extern void printf_init(void);

void bios_init(void)
{
	int i;

	/* Initialize timer */
	bios_timer_init();

	for (i = 0; i < 256; i++)
		KbdMap[i] = i;

	bios_adjust_screen();
	printf_init();

	/* Init the memory subsystem */
	bios_free_mem = (uint16_t *)0x413;
	mem_init();

	/* CPU-dependent initialization and related checks. */
	check_escapes();
}

struct firmware bios_fw = {
	.init = bios_init,
	.scan_memory = bios_scan_memory,
	.adjust_screen = bios_adjust_screen,
	.cleanup = bios_cleanup_hardware,
	.disk_init = bios_disk_init,
	.o_ops = &bios_output_ops,
	.i_ops = &bios_input_ops,
	.ipappend_strings = bios_ipappend_strings,
	.get_serial_console_info = bios_get_serial_console_info,
	.adv_ops = &bios_adv_ops,
};

void syslinux_register_bios(void)
{
	firmware = &bios_fw;
}
