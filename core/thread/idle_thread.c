#include "thread.h"
#include <limits.h>
#include <sys/cpu.h>

void (*idle_hook)(void);

static void idle_thread_func(void *dummy)
{
    (void)dummy;
    sti();

    for (;;) {
	thread_yield();
	if (idle_hook)
	    idle_hook();
	else
	    asm volatile("hlt");
    }
}

void start_idle_thread(void)
{
    start_thread("idle", 4096, INT_MAX, idle_thread_func, NULL);
}

