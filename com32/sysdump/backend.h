#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <zlib.h>
#include "serial.h"

/* Backend flags */
#define BE_NEEDLEN	0x01

struct backend {
    const char *name;
    const char *helpmsg;
    int minargs;

    size_t dbytes;
    size_t zbytes;
    const char **argv;

    uint32_t now;

    int (*write)(struct backend *);

    z_stream zstream;
    char *outbuf;
    size_t alloc;
};

/* zout.c */
int init_data(struct backend *be, const char *argv[]);
int write_data(struct backend *be, const void *buf, size_t len);
int flush_data(struct backend *be);

/* cpio.c */
#define cpio_init init_data
int cpio_hdr(struct backend *be, uint32_t mode, size_t datalen,
	     const char *filename);
int cpio_mkdir(struct backend *be, const char *filename);
int cpio_writefile(struct backend *be, const char *filename,
		   const void *data, size_t len);
int cpio_close(struct backend *be);
#define MODE_FILE	0100644
#define MODE_DIR	0040755

/* backends.c */
struct backend *get_backend(const char *name);

/* backends */
extern struct backend be_tftp;
extern struct backend be_ymodem;
extern struct backend be_srec;

#endif /* BACKEND_H */
