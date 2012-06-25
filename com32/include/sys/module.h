/**
 * syslinux/module.h
 *
 * Dynamic ELF modules definitions and services.
 */

#ifndef MODULE_H_
#define MODULE_H_

#if __SIZEOF_POINTER__ == 4
#include <i386/module.h>
#elif __SIZEOF_POINTER__ == 8
#include <x86_64/module.h>
#else
#error "unsupported architecture"
#endif

#endif
