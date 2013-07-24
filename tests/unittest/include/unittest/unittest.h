#ifndef _UNITTEST_H_
#define _UNITTEST_H_

#include </usr/include/sys/types.h>
#include </usr/include/stdint.h>
#include </usr/include/assert.h>
#include </usr/include/stdlib.h>
#include </usr/include/stdio.h>
#include </usr/include/stdint.h>

/*
 * Provide a version of assert() that prints helpful error messages when
 * the condition is false, but doesn't abort the running program.
 */
#define syslinux_assert(condition, ...) \
    if (!condition) {\
	fprintf(stderr, "Assertion failed at %s:%d: \"", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\"\n"); \
    }

#define syslinux_assert_str(condition, ...) \
    if (!(condition)) { \
	fprintf(stderr, "Assertion failed at %s:%d: \"", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\", expr \"%s\"\n", __STRING(condition)); \
    }

#endif /* _UNITTEST_H_ */
