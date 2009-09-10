#ifndef _THREAD_H
#define _THREAD_H

#include <stddef.h>
#include <inttypes.h>
#include "core.h"

struct semaphore;

struct thread_list {
    struct thread_list *next, *prev;
};

struct thread_block {
    struct thread_list list;
    struct thread *thread;
    struct semaphore *semaphore;
    mstime_t block_time;
    mstime_t timeout;
    bool timed_out;
};

struct sys_timeouts;

struct thread {
    void *esp;			/* Must be first; stack pointer */
    struct thread_list  list;
    struct thread_block *blocked;
    struct sys_timeouts *timeouts; /* For the benefit of lwIP */
    int prio;
};

void __schedule(void);
void __switch_to(struct thread *);
void thread_yield(void);

extern struct thread *__current;
static inline struct thread *current(void)
{
    return __current;
}

struct semaphore {
    int count;
    struct thread_list list;
};

mstime_t sem_down(struct semaphore *, mstime_t);
void sem_up(struct semaphore *);
void sem_init(struct semaphore *, int);

typedef unsigned long irq_state_t;

static inline irq_state_t irq_state(void)
{
    irq_state_t __st;

    asm volatile("pushfl ; popl %0" : "=rm" (__st));
    return __st;
}

static inline irq_state_t irq_save(void)
{
    irq_state_t __st;

    asm volatile("pushfl ; popl %0 ; cli" : "=rm" (__st));
    return __st;
}

static inline void irq_restore(irq_state_t __st)
{
    asm volatile("pushl %0 ; popfl" : : "rm" (__st));
}

struct thread *start_thread(size_t stack_size, int prio,
			    void (*start_func)(void *), void *func_arg);
void __exit_thread(void);
void kill_thread(struct thread *);

void start_idle_thread(void);
void test_thread(void);

#endif /* _THREAD_H */
