#ifndef MYSTUFF_H
#define MYSTUFF_H

#define NULL ((void *)0)

unsigned int skip_atou(const char **s);
unsigned int atou(const char *s);

static inline int
isdigit(int ch)
{
  return (ch >= '0') && (ch <= '9');
}

#endif /* MYSTUFF_H */
