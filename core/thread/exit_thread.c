#include "thread.h"
#include <limits.h>
#include <stdlib.h>
#include <klibc/compiler.h>

__noreturn __exit_thread(void)
{
    irq_state_t irq;
    struct thread *curr = current();

    irq = irq_save();

    /* Remove from the linked list */
    curr->list.prev->next = curr->list.next;
    curr->list.next->prev = curr->list.prev;

    /* Free allocated stacks (note: free(NULL) is permitted and safe). */
    free(curr->stack);
    free(curr->rmstack);

    /*
     * Note: __schedule() can explictly handle the case where
     * curr isn't part of the linked list anymore, as long as
     * curr->list.next is still valid.
     */
    __schedule();

    /* We should never get here */
    kaboom();
}
