/*
 * mbox.h
 *
 * Simple thread mailbox interface
 */

#ifndef _MBOX_H
#define _MBOX_H

#include "thread.h"

struct mailbox {
    struct semaphore prod_sem;	/* Producer semaphore (empty slots) */
    struct semaphore cons_sem;	/* Consumer semaphore (data slots) */
    struct semaphore head_sem;	/* Head pointer semaphore */
    struct semaphore tail_sem;	/* Tail pointer semaphore */
    void **wrap;		/* Where pointers wrap */
    void **head;		/* Head pointer */
    void **tail;		/* Tail pointer */

    void *data[];		/* Data array */
};

#endif /* _MBOX_H */
