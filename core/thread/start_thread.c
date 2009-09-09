#include <string.h>
#include <stdlib.h>
#include "thread.h"

extern void (*__start_thread)(void);

struct thread *start_thread(size_t stack_size, int prio,
			    void (*start_func)(void *), void *func_arg)
{
    irq_state_t irq;
    struct thread *curr, *t;
    char *stack;
    const size_t thread_mask = __alignof__(struct thread)-1;

    stack_size = (stack_size + thread_mask) & ~thread_mask;
    stack = malloc(stack_size + sizeof(struct thread));
    if (!stack)
	return NULL;

    t = (struct thread *)(stack + stack_size);

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
    return t;
}
