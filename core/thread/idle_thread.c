#include "thread.h"
#include <limits.h>
#include <sys/cpu.h>

static void idle_thread_func(void *dummy)
{
    (void)dummy;

    for (;;) {
	cli();
	__schedule();
	asm volatile("sti ; hlt" : : : "memory");
    }
}

void start_idle_thread(void)
{
    start_thread("idle", 4096, IDLE_THREAD_PRIORITY, idle_thread_func, NULL);
}

