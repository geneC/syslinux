/*
 * string.h
 */

#ifndef _STRING_H
#define _STRING_H

/* Standard routines */
static inline void *memcpy(void *__d, const void *__s, unsigned int __n)
{
  asm volatile("cld ; rep ; movsb"
	       : "+D" (__d), "+S" (__s), "+c" (__n));
  return __d;
}

static inline void *memset(void *__d, int __c, unsigned int __n)
{
  asm volatile("cld ; rep ; stosb"
	       : "+D" (__d), "+a" (__c), "+c" (__n));
  return __d;
}

#define strcpy(a,b)   __builtin_strcpy(a,b)
#define strlen(a)     __builtin_strlen(a)

/* This only returns true or false */
static inline int memcmp(const void *__m1, const void *__m2, unsigned int __n)
{
  _Bool rv;
  asm volatile("cld ; repe ; cmpsb ; setne %0"
	       : "=abd" (rv), "+D" (__m1), "+S" (__m2), "+c" (__n));
  return rv;
}

#endif /* _STRING_H */
