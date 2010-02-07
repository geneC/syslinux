/*
 * Compress input and feed it to a block-oriented back end.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <zlib.h>
#include "backend.h"

int init_data(struct backend *be, const char *argv[])
{
    be->outbuf = malloc(be->blocksize);
    if (!be->outbuf)
	return -1;

    if (be->open(be, argv))
	return -1;

    memset(&be->zstream, 0, sizeof be->zstream);

    be->zstream.next_out  = (void *)be->outbuf;
    be->zstream.avail_out = be->blocksize;

    /* Initialize a gzip data stream */
    if (deflateInit2(&be->zstream, 9, Z_DEFLATED,
		     16+15, 9, Z_DEFAULT_STRATEGY) < 0)
	return -1;

    return 0;
}

int write_data(struct backend *be, const void *buf, size_t len, bool flush)
{
    int rv = Z_OK;

    be->zstream.next_in = buf;
    be->zstream.avail_in = len;

    while (be->zstream.avail_in || (flush && rv == Z_OK)) {
	rv = deflate(&be->zstream, flush ? Z_FINISH : Z_NO_FLUSH);
	if (be->zstream.avail_out == 0) {
	    if (be->write(be, be->outbuf, be->blocksize))
		return -1;
	    be->zstream.next_out  = (void *)be->outbuf;
	    be->zstream.avail_out = be->blocksize;
	}
	if (rv == Z_STREAM_ERROR)
	    return -1;
    }

    if (flush) {
	/* Output the last (fractional) packet... may be zero */
	if (be->write(be, be->outbuf, be->blocksize - be->zstream.avail_out))
	    return -1;
	free(be->outbuf);
    }

    return 0;
}
