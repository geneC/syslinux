#include <sys/cpu.h>
#include "thread.h"

void sem_init(struct semaphore *sem, int count)
{
    sem->list.next = sem->list.prev = &sem->list;
    sem->count = count;
}

mstime_t __sem_down_slow(struct semaphore *sem, mstime_t timeout)
{
    struct thread *curr;
    struct thread_block block;
    irq_state_t irq;
    mstime_t now;

    irq = irq_save();

    /* Check if something already freed the semaphore on us */
    if (sem->count >= 0) {
	sti();
	return 0;
    }

    curr = current();
    now = ms_timer();

    block.thread     = curr;
    block.semaphore  = sem;
    block.block_time = now;
    block.timeout    = timeout ? now+timeout : 0;
    block.timed_out  = false;

    curr->blocked    = &block;

    /* Add to the end of the wakeup list */
    block.list.prev       = sem->list.prev;
    block.list.next       = &sem->list;
    sem->list.prev        = &block.list;
    block.list.prev->next = &block.list;

    __schedule();

    irq_restore(irq);
    return block.timed_out ? -1 : ms_timer() - block.block_time;
}

void __sem_up_slow(struct semaphore *sem)
{
    irq_state_t irq;
    struct thread_list *l;

    irq = irq_save();

    /*
     * It's possible that something did a down on the semaphore, but
     * didn't get to add themselves to the queue just yet.  In that case
     * we don't have to do anything, since the bailout clause in
     * __sem_down_slow will take care of it.
     */
    l = sem->list.next;
    if (l != &sem->list) {
	struct thread_block *block = container_of(l, struct thread_block, list);

	sem->list.next = block->list.next;
	block->list.next->prev = &sem->list;

	block->thread->blocked = NULL;

	__schedule();
    }

    irq_restore(irq);
}
