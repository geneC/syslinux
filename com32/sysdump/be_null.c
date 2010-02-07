/*
 * Null data output backend
 */

#include "backend.h"

static int be_null_open(struct backend *be, const char *argv[], size_t len)
{
    (void)be;
    (void)argv;
    (void)len;
    return 0;
}

static int be_null_write(struct backend *be, const char *buf, size_t len)
{
    (void)be;
    (void)buf;
    (void)len;
    return 0;
}

struct backend be_null = {
    .name       = "null",
    .helpmsg    = "",
    .minargs    = 0,
    .blocksize	= 32768,	/* arbitrary */
    .open       = be_null_open,
    .write      = be_null_write,
};
