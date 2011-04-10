#ifndef _BYTESWAP_H
#define _BYTESWAP_H

/* COM32 will be running on an i386 platform */

#include <stdint.h>
#include <klibc/compiler.h>

#define __bswap_16_macro(v) ((uint16_t)		  			\
			     (((uint16_t)(v) << 8) | 			\
			      ((uint16_t)(v) >> 8)))

static inline __constfunc uint16_t __bswap_16(uint16_t v)
{
    return __bswap_16_macro(v);
}

#define bswap_16(x) (__builtin_constant_p(x) ?				\
			__bswap_16_macro(x) : __bswap_16(x))

#define __bswap_32_macro(v) ((uint32_t)					\
			     ((((uint32_t)(v) & 0x000000ff) << 24) |	\
			      (((uint32_t)(v) & 0x0000ff00) << 8)  |	\
			     (((uint32_t)(v) & 0x00ff0000) >> 8)  |	\
			      (((uint32_t)(v) & 0xff000000) >> 24)))

static inline __constfunc uint32_t __bswap_32(uint32_t v)
{
    asm("xchgb %h0,%b0 ; roll $16,%0 ; xchgb %h0,%b0"
	: "+q" (v));
    return v;
}

#define bswap_32(x) (__builtin_constant_p(x) ?				\
			 __bswap_32_macro(x) : __bswap_32(x))


#define __bswap_64_macro(v) ((uint64_t)					\
    (((uint64_t)__bswap_32_macro((uint32_t)(v)) << 32) |		\
     (__bswap_32__macro((uint32_t)((uint64_t)(v) >> 32)))))

static inline __constfunc uint64_t __bswap_64(uint64_t v)
{
    return ((uint64_t)__bswap_32(v) << 32) | __bswap_32(v >> 32);
}

#define bswap_64(x) (__builtin_constant_p(x) ? 				\
			__bswap_64_macro(x) :  __bswap_64(x))

#endif /* byteswap.h */
