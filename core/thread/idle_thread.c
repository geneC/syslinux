#include "thread.h"
#include <limits.h>
#include <sys/cpu.h>

static void idle_thread_func(void *dummy)
{
    (void)dummy;
    sti();

    for (;;) {
	thread_yield();
	asm volatile("hlt");
    }
}

void start_idle_thread(void)
{
    start_thread(4096, INT_MAX, idle_thread_func, NULL);
}

