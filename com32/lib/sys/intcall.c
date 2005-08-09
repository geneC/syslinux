/*
 * intcall.c
 */

#include <com32.h>

void __intcall(uint8_t __i, const com32sys_t *__sr, com32sys_t *__dr)
{
  __com32.cs_intcall(__i, __sr, __dr);
}
