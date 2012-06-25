/*
 * bits32/stdint.h
 */

#ifndef _BITSIZE_STDINT_H
#define _BITSIZE_STDINT_H

/*
typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef int int_fast16_t;
typedef int int_fast32_t;

typedef unsigned int uint_fast16_t;
typedef unsigned int uint_fast32_t;

typedef int intptr_t;
typedef unsigned int uintptr_t;

#define __INT64_C(c)   c ## LL
#define __UINT64_C(c)  c ## ULL

#define __PRI64_RANK   "ll"
#define __PRIFAST_RANK ""
#define __PRIPTR_RANK  ""
*/

/* Exact types */

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

/* Small types */

typedef signed char int_least8_t;
typedef signed short int_least16_t;
typedef signed int int_least32_t;
typedef signed long long int_least64_t;

typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned int uint_least32_t;
typedef unsigned long long uint_least64_t;

/* Fast types */

typedef signed char int_fast8_t;
typedef signed long long int_fast64_t;

typedef unsigned char uint_fast8_t;
typedef unsigned int uint_fast32_t;
typedef unsigned long long uint_fast64_t;

/* Maximal types */

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#if __SIZEOF_POINTER__ == 4
#include <bitsize32/stdint.h>
#elif __SIZEOF_POINTER__ == 8
#include <bitsize64/stdint.h>
#else
#error "Unable to build for to-be-defined architecture type"
#endif
#endif /* _BITSIZE_STDINT_H */
