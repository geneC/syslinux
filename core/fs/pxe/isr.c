/*
 * core/fs/pxe/isr.c
 *
 * Stub invoked on return from real mode including from an interrupt.
 * Interrupts are locked out on entry.
 */

#include "core.h"
#include "thread.h"
#include "pxe.h"
#include <string.h>

extern uint8_t pxe_irq_pending;
static DECLARE_INIT_SEMAPHORE(pxe_receive_thread_sem, 0);

static void pm_return(void)
{
    static jiffies_t last_jiffies = 0;
    jiffies_t now = jiffies();
    
    __schedule_lock++;

    if (now != last_jiffies) {
	last_jiffies = now;
	__thread_process_timeouts();
	__need_schedule = true; /* Switch threads if more than one runnable */
    }

    if (pxe_irq_pending) {
	pxe_irq_pending = 0;
	sem_up(&pxe_receive_thread_sem);
    }

    __schedule_lock--;

    if (__need_schedule)
	__schedule();
}

void undiif_input(t_PXENV_UNDI_ISR *);

static void pxe_receive_thread(void *dummy)
{
    static __lowmem t_PXENV_UNDI_ISR isr;
    uint16_t func;
    bool done;

    (void)dummy;

    for (;;) {
	sem_down(&pxe_receive_thread_sem, 0);
	func = PXENV_UNDI_ISR_IN_PROCESS; /* First time */

	done = false;
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
}


void pxe_init_isr(void)
{
    start_idle_thread();
    start_thread("pxe receive", 16384, 0, pxe_receive_thread, NULL);
    core_pm_hook = pm_return;
}
