#include "arch/sys_arch.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include <stdlib.h>
#include <thread.h>

void sys_init(void)
{
}

sys_sem_t sys_sem_new(u8_t count)
{
    sys_sem_t sem = malloc(sizeof(struct semaphore));
    if (!sem)
	return NULL;

    sem_init(sem, count);
    return sem;
}

void sys_sem_free(sys_sem_t sem)
{
    free(sem);
}

u32_t sys_arch_sem_wait(sys_sem_t sem, u32_t timeout)
{
    mstime_t rv;

    rv = sem_down(sem, timeout);
    if (rv == (mstime_t)-1)
	return SYS_ARCH_TIMEOUT;
    else
	return rv;
}

sys_mbox_t sys_mbox_new(int size)
{
    struct mailbox *mbox;

    mbox = malloc(MBOX_BYTES(size));
    if (!mbox)
	return NULL;

    mbox_init(mbox, size);
    return mbox;
}

void sys_mbox_free(sys_mbox_t mbox)
{
    free(mbox);
}

void sys_mbox_post(sys_mbox_t mbox, void *msg)
{
    mbox_post(mbox, msg, 0);
}

err_t sys_mbox_trypost(sys_mbox_t mbox, void *msg)
{
    return mbox_post(mbox, msg, -1);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t mbox, void **msg, u32_t timeout)
{
    mstime_t rv;

    rv = mbox_fetch(mbox, msg, timeout);
    if (rv == (mstime_t)-1)
	return SYS_ARCH_TIMEOUT;
    else
	return rv;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t mbox, void **msg)
{
    return mbox_fetch(mbox, msg, -1);
}

sys_thread_t sys_thread_new(char *name, void (*thread)(void *),
			     void *arg, int stacksize, int prio)
{
    return start_thread(name, stacksize, prio, thread, arg);
}

