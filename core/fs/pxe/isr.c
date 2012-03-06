/*
 * core/fs/pxe/isr.c
 *
 * Stub invoked on return from real mode including from an interrupt.
 * Interrupts are locked out on entry.
 */

#if DEBUG < 2
#  undef DEBUG
#  define DEBUG 2
#endif

#include "core.h"
#include "thread.h"
#include "pxe.h"
#include <string.h>
#include <sys/cpu.h>
#include <sys/io.h>
#include <dprintf.h>

extern uint8_t pxe_irq_pending;
extern uint32_t pxe_irq_count, pxe_irq_count_us;
static DECLARE_INIT_SEMAPHORE(pxe_receive_thread_sem, 0);
static struct thread *pxe_thread, *poll_thread;

static bool install_irq_vector(uint8_t irq, void (*isr)(void), far_ptr_t *old)
{
    far_ptr_t *entry;
    unsigned int vec;
    uint8_t mask;
    uint16_t maskreg;
    irq_state_t irqstate;

    if (irq < 8)
	vec = irq + 0x08;
    else if (irq < 16)
	vec = (irq - 8) + 0x70;
    else
	return false;
    pxe_irq_count_us = pxe_irq_count = 0;

    irqstate = irq_save();

    entry = (far_ptr_t *)(vec << 2);
    *old = *entry;
    entry->ptr = (uint32_t)isr;

    /* Enable this interrupt at the PIC level, just in case... */
    maskreg = 0x21 + ((irq & 8) << (7-3));
    mask = inb(maskreg);
    mask &= ~(1 << (irq & 3));
    outb(mask, maskreg);

    irq_restore(irqstate);

    printf("UNDI: IRQ %d(0x%02x): %04x:%04x -> %04x:%04x\n", irq, vec,
	   old->seg, old->offs, entry->seg, entry->offs);
    dprintf("UNDI: IRQ %d(0x%02x): %04x:%04x -> %04x:%04x\n", irq, vec,
	   old->seg, old->offs, entry->seg, entry->offs);

    return true;
}

static bool uninstall_irq_vector(uint8_t irq, void (*isr), far_ptr_t *old)
{
    far_ptr_t *entry;
    unsigned int vec;

    if (!irq)
	return true;		/* Nothing to uninstall */

    if (irq < 8)
	vec = irq + 0x08;
    else if (irq < 16)
	vec = (irq - 8) + 0x70;
    else
	return false;

    entry = (far_ptr_t *)(vec << 2);

    if (entry->ptr != (uint32_t)isr)
	return false;

    *entry = *old;
    return true;
}

static void pxe_poll_wakeups(void)
{
    static jiffies_t last_jiffies = 0;
    jiffies_t now = jiffies();

    if (now != last_jiffies) {
	last_jiffies = now;
	__thread_process_timeouts();
    }

    if (pxe_irq_pending) {
	pxe_irq_pending = 0;
	sem_up(&pxe_receive_thread_sem);
    }
}

static void pxe_process_irq(void)
{
    static __lowmem t_PXENV_UNDI_ISR isr;

    uint16_t func = PXENV_UNDI_ISR_IN_PROCESS; /* First time */
    bool done = false;

    while (!done) {
        memset(&isr, 0, sizeof isr);
        isr.FuncFlag = func;
        func = PXENV_UNDI_ISR_IN_GET_NEXT; /* Next time */

        pxe_call(PXENV_UNDI_ISR, &isr);

        switch (isr.FuncFlag) {
        case PXENV_UNDI_ISR_OUT_DONE:
	    done = true;
	    break;

        case PXENV_UNDI_ISR_OUT_TRANSMIT:
	    /* Transmit complete - nothing for us to do */
	    break;

        case PXENV_UNDI_ISR_OUT_RECEIVE:
	    dprintf("pxe_process_irq(): pxe_irq_count %08X/%08X\n", pxe_irq_count, pxe_irq_count_us);
	    undiif_input(&isr);
	    break;

        case PXENV_UNDI_ISR_OUT_BUSY:
	/* ISR busy, this should not happen */
	    done = true;
	    break;

        default:
	/* Invalid return code, this should not happen */
	    done = true;
	    break;
        }
    }
}

static void pxe_receive_thread(void *dummy)
{
    (void)dummy;

    for (;;) {
	sem_down(&pxe_receive_thread_sem, 0);
	pxe_process_irq();
    }
}

static bool pxe_isr_poll(void)
{
    static __lowmem t_PXENV_UNDI_ISR isr;

    isr.FuncFlag = PXENV_UNDI_ISR_IN_START;
    pxe_call(PXENV_UNDI_ISR, &isr);

    return isr.FuncFlag == PXENV_UNDI_ISR_OUT_OURS;
}

static void pxe_poll_thread(void *dummy)
{
    (void)dummy;

    for (;;) {
	thread_yield();
	if (pxe_isr_poll())
	    sem_up(&pxe_receive_thread_sem);
    }
}

/*
 * This does preparations and enables the PXE thread
 */
void pxe_init_isr(void)
{
    start_idle_thread();
    sched_hook_func = pxe_poll_wakeups;
    /*
     * Run the pxe receive thread at elevated priority, since the UNDI
     * stack is likely to have very limited memory available; therefore to
     * avoid packet loss we need to move it into memory that we ourselves
     * manage, as soon as possible.
     */
    pxe_thread = start_thread("pxe receive", 16384, -20,
			      pxe_receive_thread, NULL);
    core_pm_hook = __schedule;
}

/*
 * Actually start the interrupt routine inside the UNDI stack
 */
void pxe_start_isr(void)
{
    pxe_irq_vector = pxe_undi_info.IntNumber; /* Used in 16-bit code */

    if (pxe_undi_info.IntNumber)
	install_irq_vector(pxe_undi_info.IntNumber, pxe_isr, &pxe_irq_chain);
    
    if (!pxe_undi_info.IntNumber ||
	1 ||
	!(pxe_undi_iface.ServiceFlags & PXE_UNDI_IFACE_FLAG_IRQ))
	poll_thread = start_thread("pxe poll", 4096, POLL_THREAD_PRIORITY,
				   pxe_poll_thread, NULL);
}

int reset_pxe(void)
{
    static __lowmem struct s_PXENV_UNDI_CLOSE undi_close;

    sched_hook_func = NULL;
    core_pm_hook = core_pm_null_hook;
    kill_thread(pxe_thread);

    memset(&undi_close, 0, sizeof(undi_close));
    pxe_call(PXENV_UNDI_CLOSE, &undi_close);

    if (undi_close.Status)
	printf("PXENV_UNDI_CLOSE failed: 0x%x\n", undi_close.Status);

    if (pxe_undi_info.IntNumber)
	uninstall_irq_vector(pxe_undi_info.IntNumber, pxe_isr, &pxe_irq_chain);
    if (poll_thread)
	kill_thread(poll_thread);

    return undi_close.Status;
}
