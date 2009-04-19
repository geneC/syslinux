#ifndef _UTIL_H_
#define _UTIL_H_

#include <com32.h>

int int13_retry(const com32sys_t *inreg, com32sys_t *outreg);
void get_error(const int, char**);

#endif /* _UTIL_H_ */
