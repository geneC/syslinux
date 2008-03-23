/*
 * memset.c
 */

#include <string.h>
#include <stdint.h>

void *memset(void *dst, int c, size_t n)
{
  char *q = dst;
  size_t nl = n >> 2;

  asm volatile("rep ; stosl ; movl %3,%0 ; rep ; stosb"
	       : "+c" (nl), "+D" (q)
	       : "a" ((unsigned char)c * 0x01010101U), "r" (n & 3));

  return dst;
}
