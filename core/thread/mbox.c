/*
 * mbox.c
 *
 * Simple thread mailbox interface
 */

#include "thread.h"
#include "mbox.h"
#include <errno.h>

void mbox_init(struct mailbox *mbox, size_t size)
{
    sem_init(&mbox->prod_sem, size); /* All slots empty */
    sem_init(&mbox->cons_sem, 0);    /* No slots full */
    sem_init(&mbox->head_sem, 1);    /* Head mutex */
    sem_init(&mbox->tail_sem, 1);    /* Tail mutex */

    mbox->wrap = &mbox->data[size];
    mbox->head = &mbox->data[0];
    mbox->tail = &mbox->data[0];
};

static void mbox_post_common(struct mailbox *mbox, void *msg)
{
    sem_down(&mbox->head_sem, 0);

    *mbox->head = msg;
    mbox->head++;
    if (mbox->head == mbox->wrap)
	mbox->head = &mbox->data[0];

    sem_up(&mbox->head_sem);
    sem_up(&mbox->cons_sem);
}

void mbox_post(struct mailbox *mbox, void *msg)
{
    sem_down(&mbox->prod_sem, 0);
    mbox_post_common(mbox, msg);
}

int mbox_trypost(struct mailbox *mbox, void *msg)
{
    if (sem_down(&mbox->prod_sem, -1) == (mstime_t)-1)
	return ENOMEM;
    mbox_post_common(mbox, msg);
    return 0;
}

mstime_t mbox_fetch(struct mailbox *mbox, void **msg, mstime_t timeout)
{
    mstime_t t;

    t = sem_down(&mbox->cons_sem, timeout);
    if (t == (mstime_t)-1)
	return -1;
    t += sem_down(&mbox->tail_sem, 0);

    if (msg)
	*msg = *mbox->tail;
    mbox->tail++;
    if (mbox->tail == mbox->wrap)
	mbox->tail = &mbox->data[0];

    sem_up(&mbox->tail_sem);
    sem_up(&mbox->prod_sem);
    return t;
}
