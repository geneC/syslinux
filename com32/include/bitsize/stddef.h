/*
 * Include stddef.h as appropriate for architecture
 */

#ifndef _BITSIZE_STDDEF_H
#define _BITSIZE_STDDEF_H

#define _SIZE_T
#if __SIZEOF_POINTER__ == 4
#include <bitsize32/stddef.h>
#elif __SIZEOF_POINTER__ == 8
#include <bitsize64/stddef.h>
#else
#error "Unable to build for to-be-defined architecture type"
#endif
/* Original definitions below */
/*
#if defined(__s390__) || defined(__hppa__) || defined(__cris__)
typedef unsigned long size_t;
#else
typedef unsigned int size_t;
#endif

#define _PTRDIFF_T
typedef signed int ptrdiff_t;
*/

#else
#error "BITSIZE_STDDEF already defined"
#endif /* _BITSIZE_STDDEF_H */
