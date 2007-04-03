/*
 * farcall.c
 */

#include <com32.h>

void __farcall(uint16_t cs, uint16_t ip,
	       const com32sys_t *ireg, com32sys_t *oreg)
{
  __com32.cs_farcall((cs << 16)+ip, ireg, oreg);
}
