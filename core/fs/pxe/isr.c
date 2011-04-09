/*
 * core/fs/pxe/isr.c
 *
 * Stub invoked on return from real mode including from an interrupt.
 * Interrupts are locked out on entry.
 */

#include "core.h"
#include "thread.h"

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
    }

    if (pxe_irq_pending) {
	pxe_irq_pending = 0;
	sem_up(&pxe_receive_thread_sem);
    }

    __schedule_lock--;

    if (__need_schedule)
	__schedule();
}

void pxe_init_isr(void)
{
    start_idle_thread();
    core_pm_hook = pm_return;
}
