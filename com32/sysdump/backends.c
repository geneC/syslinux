/*
 * backends.c
 */

#include <string.h>
#include "backend.h"

extern struct backend be_tftp;

static struct backend *backends[] =
{
    &be_tftp,
    NULL
};

struct backend *get_backend(const char *name)
{
    struct backend *be, **bep;

    for (bep = backends ; (be = *bep) ; bep++) {
	if (!strcmp(name, be->name))
	    return be;
    }

    return NULL;
}

