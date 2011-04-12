#include <sys/cpu.h>
#include "thread.h"

int __schedule_lock;
bool __need_schedule;
void (*sched_hook_func)(void);

/*
 * __schedule() should only be called with interrupts locked out!
 */
void __schedule(void)
{
    struct thread *curr = current();
    struct thread *st, *nt, *best;

    if (__schedule_lock) {
	__need_schedule = true;
	return;
    }

    /* Possibly update the information on which we make
     * scheduling decisions.
     */
    if (sched_hook_func) {
	__schedule_lock++;
	sched_hook_func();
	__schedule_lock--;
    }

    __need_schedule = false;

    best = NULL;

    /*
     * The unusual form of this walk is because we have to start with
     * the thread *following* curr, and curr may not actually be part
     * of the list anymore (in the case of __exit_thread).
     */
    nt = st = container_of(curr->list.next, struct thread, list);
    do {
	if (!nt->blocked)
	    if (!best || nt->prio < best->prio)
		best = nt;
	nt = container_of(nt->list.next, struct thread, list);
    } while (nt != st);

    if (best != curr)
	__switch_to(best);
}

/*
 * This can be called from "normal" code...
 */
void thread_yield(void)
{
    irq_state_t irq = irq_save();
    __schedule();
    irq_restore(irq);
}
