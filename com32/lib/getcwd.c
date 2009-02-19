/*
 * getcwd.c
 */

#include <syslinux/config.h>
#include <klibc/compiler.h>
#include <com32.h>

#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

char *getcwd(char *buf, size_t size)
{
	static com32sys_t reg;
	char *pwdstr, *ret;

	reg.eax.w[0] = 0x001f;
	__intcall(0x22, &reg, &reg);
	pwdstr =  MK_PTR(reg.es, reg.ebx.w[0]);
	if ((strlen(pwdstr) < size) && (buf != NULL)) {
		strcpy(buf, pwdstr);
		ret = buf;
	} else {
		ret = NULL;
		errno = ERANGE;
	}
	return ret;
}
