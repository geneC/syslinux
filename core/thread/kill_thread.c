#include "thread.h"
#include <limits.h>

void kill_thread(struct thread *thread)
{
    irq_state_t irq;
    struct thread_block *block;

    if (thread == current())
	__exit_thread();

    irq = irq_save();

    /*
     * Muck with the stack so that the next time the thread is run then
     * we end up going to __exit_thread.
     */
    *(size_t *)thread->state.esp = (size_t)__exit_thread;
    thread->prio = INT_MIN;

    block = thread->blocked;
    if (block) {
	struct semaphore *sem = block->semaphore;
	/* Remove us from the queue and increase the count */
	block->list.next->prev = block->list.prev;
	block->list.prev->next = block->list.next;
	sem->count++;

	thread->blocked = NULL;
	block->timed_out = true; /* Fake an immediate timeout */
    }

    __schedule();

    irq_restore(irq);
}



