/*
 * farcall.c
 */

#include <com32.h>

void __farcall(uint16_t __es, uint16_t __eo,
	       const com32sys_t *__sr, com32sys_t *__dr)
{
  __com32.cs_farcall((__es << 16) + __eo, __sr, __dr);
}
