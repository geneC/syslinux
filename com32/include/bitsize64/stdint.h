/*
 * bits64/stdint.h
 */


/*
typedef signed char		int8_t;
typedef short int		int16_t;
typedef int			int32_t;
typedef long int		int64_t;

typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long int	uint64_t;

typedef long int		int_fast16_t;
typedef long int		int_fast32_t;

typedef unsigned long int	uint_fast16_t;
typedef unsigned long int	uint_fast32_t;

typedef long int		intptr_t;
typedef unsigned long int	uintptr_t;

#define __INT64_C(c)  c ## L
#define __UINT64_C(c) c ## UL

#define __PRI64_RANK	"l"
#define __PRIFAST_RANK  "l"
#define __PRIPTR_RANK	"l"
*/

/* based on compiler output */
typedef signed long int_fast16_t;
typedef signed long int int_fast32_t;
typedef unsigned long int uint_fast16_t;
/* Pointer types */

typedef long int intptr_t;
typedef long unsigned int uintptr_t;
