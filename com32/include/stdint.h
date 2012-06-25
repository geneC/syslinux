/*
 * stdint.h
 */

#ifndef _STDINT_H
#define _STDINT_H

/* FIXME: Move common typedefs to bitsize/stdint.h
 * and architecture specificis to bitsize32/64
 */
#include <bitsize/stdint.h>
#if 0
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
typedef signed short int_fast16_t;	/* ?? 32: int 64: long */
typedef signed int int_fast32_t;	/* ?? 64: long int */
typedef signed long long int_fast64_t;	/* 64: long int */

typedef unsigned char uint_fast8_t;
typedef unsigned short uint_fast16_t;	/* 32: unsigned int 64: short unsigned int */
typedef unsigned int uint_fast32_t;
typedef unsigned long long uint_fast64_t;/* 64: long unsigned int */

/* Pointer types */

typedef int32_t intptr_t;	/* 64: long int */
typedef uint32_t uintptr_t;	/* 64: long unsigned int */

/* Maximal types */

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#endif
/* FIXME: move common definitions to bitsize/stdint.h and
 * architecture specifics to bitsize32/64/stdintlimits.h as appropriate
 */
/*
 * To be strictly correct...
 */
#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)

# define INT8_MIN               (-128)
# define INT16_MIN              (-32767-1)
# define INT32_MIN              (-2147483647-1)
# define INT64_MIN              (-9223372036854775807LL-1)

# define INT8_MAX               (127)
# define INT16_MAX              (32767)
# define INT32_MAX              (2147483647)
# define INT64_MAX              (9223372036854775807LL)

# define UINT8_MAX              (255U)
# define UINT16_MAX             (65535U)
# define UINT32_MAX             (4294967295U)
# define UINT64_MAX             (18446744073709551615ULL)

# define INT_LEAST8_MIN               INT8_MIN //(-128)
# define INT_LEAST16_MIN              INT16_MIN //(-32767-1)
# define INT_LEAST32_MIN              INT32_MIN //(-2147483647-1)
# define INT_LEAST64_MIN              INT64_MIN //(-9223372036854775807LL-1)

# define INT_LEAST8_MAX               INT8_MAX //(127)
# define INT_LEAST16_MAX              INT16_MAX //(32767)
# define INT_LEAST32_MAX              INT32_MAX //(2147483647)
# define INT_LEAST64_MAX              INT64_MAX //(9223372036854775807LL)

# define UINT_LEAST8_MAX              UINT8_MAX //(255U)
# define UINT_LEAST16_MAX             UINT16_MAX //(65535U)
# define UINT_LEAST32_MAX             UINT32_MAX //(4294967295U)
# define UINT_LEAST64_MAX             UINT64_MAX //(18446744073709551615ULL)

# define INT_FAST8_MIN               INT8_MIN //(-128)
//# define INT_FAST16_MIN              (-32767-1)
//# define INT_FAST32_MIN              (-2147483647-1)
# define INT_FAST64_MIN              INT64_MIN //(-9223372036854775807LL-1)

# define INT_FAST8_MAX               INT8_MAX //(127)
//# define INT_FAST16_MAX              (32767)
//# define INT_FAST32_MAX              (2147483647)
# define INT_FAST64_MAX              INT64_MAX //(9223372036854775807LL)

# define UINT_FAST8_MAX              UINT8_MAX //(255U)
//# define UINT_FAST16_MAX             (65535U)
//# define UINT_FAST32_MAX             (4294967295U)
# define UINT_FAST64_MAX             UINT64_MAX //(18446744073709551615ULL)

//FIXME: Move *INTPTR to architecture specific as they vary
//# define INTPTR_MIN		(-2147483647-1)
//# define INTPTR_MAX		(2147483647)
//# define UINTPTR_MAX		(4294967295U)

# define INTMAX_MIN		(-9223372036854775807LL-1)
# define INTMAX_MAX		(9223372036854775807LL)
# define UINTMAX_MAX		(18446744073709551615ULL)

/* FIXME: include the architecture specific limits from bitsizexx/stdintlimits.h */
/* ptrdiff_t limit */
//# define PTRDIFF_MIN		(-2147483647-1)
//# define PTRDIFF_MAX		(2147483647)

/* sig_atomic_t limit */
//# define SIG_ATOMIC_MIN         (-2147483647-1)
//# define SIG_ATOMIC_MAX         (2147483647)

/* size_t limit */
//# define SIZE_MAX		(4294967295U)

#include <bitsize/stdintlimits.h>

#endif /* STDC_LIMIT_MACROS */

#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)

# define INT8_C(n)	n
# define INT16_C(n)	n
# define INT32_C(n)	n
# define INT64_C(n)	n ## LL

# define UINT8_C(n)	n ## U
# define UINT16_C(n)	n ## U
# define UINT32_C(n)	n ## U
# define UINT64_C(n)	n ## ULL

//# define INTMAX_C(n)	n ## LL
//# define UINTMAX_C(n)	n ## ULL

/* included from klibc/usr/include/stdint.h */

#define INT_LEAST8_C(c)	 INT8_C(c)
#define INT_LEAST16_C(c) INT16_C(c)
#define INT_LEAST32_C(c) INT32_C(c)
#define INT_LEAST64_C(c) INT64_C(c)

#define UINT_LEAST8_C(c)  UINT8_C(c)
#define UINT_LEAST16_C(c) UINT16_C(c)
#define UINT_LEAST32_C(c) UINT32_C(c)
#define UINT_LEAST64_C(c) UINT64_C(c)

#define INT_FAST8_C(c)	INT8_C(c)
#define INT_FAST64_C(c) INT64_C(c)

#define UINT_FAST8_C(c)  UINT8_C(c)
#define UINT_FAST64_C(c) UINT64_C(c)

#define INTMAX_C(c)	INT64_C(c)
#define UINTMAX_C(c)	UINT64_C(c)

#include <bitsize/stdintconst.h>
#endif /* STDC_CONSTANT_MACROS */

#endif /* _STDINT_H */
