#ifndef STDLIB_H
#define STDLIB_H

#define NULL ((void *)0)

typedef int ssize_t;
typedef unsigned int size_t;

void __attribute__ ((noreturn)) exit(int);

void *malloc(size_t);
void free(void *);

#endif
