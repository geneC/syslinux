#include "thread.h"
#include <limits.h>
#include <sys/cpu.h>

static struct thread idle_thread;

static char idle_thread_stack[4096];

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
    start_thread(&idle_thread, idle_thread_stack, sizeof idle_thread_stack,
		 INT_MAX, idle_thread_func, NULL);
}

