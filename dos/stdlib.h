#ifndef STDLIB_H
#define STDLIB_H

typedef int ssize_t;
typedef unsigned int size_t;

void __attribute__ ((noreturn)) exit(int);

void *malloc(size_t);
void *calloc(size_t, size_t);
void free(void *);

extern unsigned long int strtoul(const char *nptr,
                                  char **endptr, int base);

#endif
