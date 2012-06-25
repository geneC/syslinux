#ifndef _CPU_H
#define _CPU_H

#include <stdbool.h>
#include <stdint.h>
#include <klibc/compiler.h>

#if __SIZEOF_POINTER__ == 4
#include <i386/cpu.h>
#elif __SIZEOF_POINTER__ == 8
#include <x86_64/cpu.h>
#else 
#error "unsupported architecture"
#endif

#endif
