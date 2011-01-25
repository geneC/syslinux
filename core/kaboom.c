/*
 * kaboom.c
 */

#include "core.h"

#undef kaboom

__noreturn _kaboom(void)
{
    extern void kaboom(void);
    call16(kaboom, &zero_regs, NULL);
    /* Do this if kaboom somehow returns... */
    for (;;)
	asm volatile("hlt");
}
