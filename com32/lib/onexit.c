/*
 * onexit.c
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/module.h>
#include "atexit.h"

extern __noreturn(*__exit_handler) (int);
static struct atexit *__atexit_list;

int on_exit(void (*fctn) (int, void *), void *arg)
{
    struct atexit *as = malloc(sizeof(struct atexit));

    if (!as)
	return -1;

    as->fctn = fctn;
    as->arg = arg;

    as->next = __syslinux_current->u.x.atexit_list;
    __syslinux_current->u.x.atexit_list = as;

    return 0;
}
