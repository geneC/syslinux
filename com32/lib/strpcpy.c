/*
 * strpcpy.c
 *
 * strpcpy() - strcpy() which returns a pointer to the final null
 */

#include <string.h>

char *strpcpy(char *dst, const char *src)
{
  char *q = dst;
  const char *p = src;
  char ch;

  do {
    *q++ = ch = *p++;
  } while ( ch );

  return q-1;
}
