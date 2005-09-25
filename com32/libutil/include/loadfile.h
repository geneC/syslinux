#ifndef LIBUTIL_LOADFILE_H
#define LIBUTIL_LOADFILE_H

#include <stddef.h>

/* loadfile() returns the true size of the file, but will guarantee valid,
   zero-padded memory out to this boundary. */
#define LOADFILE_ZERO_PAD	64

int loadfile(const char *, void **, size_t *);

#endif
