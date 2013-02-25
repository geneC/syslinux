#include <core.h>
#include <com32.h>
#include <sys/io.h>
#include <fs.h>
#include <bios.h>

static uint32_t min_lowmem_heap = 65536;
extern char __lowmem_heap[];
uint8_t KbdFlags;		/* Check for keyboard escapes */
__export uint8_t KbdMap[256];	/* Keyboard map */

__export uint16_t PXERetry;

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

void init(com32sys_t *regs __unused)
{
	int i;

	/* Initialize timer */
	bios_timer_init();

	for (i = 0; i < 256; i++)
		KbdMap[i] = i;

	adjust_screen();

	/* Init the memory subsystem */
	mem_init();

	/* CPU-dependent initialization and related checks. */
	check_escapes();

	/*
	 * Scan the DMI tables for interesting information.
	 */
	dmi_init();
}
