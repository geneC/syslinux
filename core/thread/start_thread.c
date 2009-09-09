#include <string.h>
#include "thread.h"

extern void (*__start_thread)(void);

void start_thread(struct thread *t, void *stack, size_t stack_size, int prio,
		  void (*start_func)(void *), void *func_arg)
{
    irq_state_t irq;
    struct thread *curr;

    memset(t, 0, sizeof *t);

    t->state.esp = (((size_t)stack + stack_size) & ~3) - 4;
    *(size_t *)t->state.esp = (size_t)&__start_thread;

    t->state.esi = (size_t)start_func;
    t->state.edi = (size_t)func_arg;
    t->state.ebx = irq_state();	/* Inherit the IRQ state from the spawner */
    t->prio = prio;

    irq = irq_save();
    curr = current();

    t->list.prev       = &curr->list;
    t->list.next       = curr->list.next;
    curr->list.next    = &t->list;
    t->list.next->prev = &t->list;

    __schedule();

    irq_restore(irq);
}
