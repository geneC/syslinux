/*
 * farcall.c
 */

#include <com32.h>

static inline uint32_t eflags(void)
{
    uint32_t v;

    asm volatile("pushfl ; popl %0" : "=rm" (v));
    return v;
}

void __farcall(uint16_t cs, uint16_t ip,
	       const com32sys_t * ireg, com32sys_t * oreg)
{
    com32sys_t xreg = *ireg;

    /* Enable interrupts if and only if they are enabled in the caller */
    xreg.eflags.l = (xreg.eflags.l & ~EFLAGS_IF) | (eflags() & EFLAGS_IF);

    __com32.cs_farcall((cs << 16) + ip, &xreg, oreg);
}
