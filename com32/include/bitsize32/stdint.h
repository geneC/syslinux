/*
 * bits32/stdint.h
 */


/*
typedef signed char		int8_t;
typedef short int		int16_t;
typedef int			int32_t;
typedef long long int		int64_t;

typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long int	uint64_t;

typedef int			int_fast16_t;
typedef int			int_fast32_t;

typedef unsigned int		uint_fast16_t;
typedef unsigned int		uint_fast32_t;

typedef int			intptr_t;
typedef unsigned int		uintptr_t;

#define __INT64_C(c)   c ## LL
#define __UINT64_C(c)  c ## ULL

#define __PRI64_RANK   "ll"
#define __PRIFAST_RANK ""
#define __PRIPTR_RANK  ""
*/

/* changes made according compiler output */
typedef signed int int_fast16_t;	/* was short */
typedef signed int int_fast32_t;
typedef unsigned int uint_fast16_t;	/* was ushort */
/* Pointer types */

typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
