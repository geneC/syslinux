/*
 * init.h
 *
 * Magic to set up initializers
 */

#ifndef _INIT_H
#define _INIT_H 1

#include <inttypes.h>

#define COM32_INIT(x) static const void * const  __COM32_INIT \
	__attribute__((section(".init_array"),unused)) = (const void * const)&x

#endif /* _INIT_H */
