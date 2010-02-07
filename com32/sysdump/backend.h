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

    unsigned int blocksize;
    unsigned int flags;

    size_t dbytes;
    size_t zbytes;

    int (*open)(struct backend *, const char *argv[], size_t len);
    int (*write)(struct backend *, const char *buf, size_t len);

    z_stream zstream;
    char *outbuf;

    union {
	struct {
	    uint32_t my_ip;
	    uint32_t srv_ip;
	    uint16_t my_port;
	    uint16_t srv_port;
	    uint16_t seq;
	} tftp;
	struct {
	    struct serial_if serial;
	    uint16_t seq;
	} ymodem;
    };
};

/* zout.c */
int init_data(struct backend *be, const char *argv[], size_t len);
int write_data(struct backend *be, const void *buf, size_t len, bool flush);

/* cpio.c */
int cpio_init(struct backend *be, const char *argv[], size_t len);
int cpio_mkdir(struct backend *be, const char *filename);
int cpio_writefile(struct backend *be, const char *filename,
		   const void *data, size_t len);
int cpio_close(struct backend *be);

/* backends.c */
struct backend *get_backend(const char *name);

/* backends */
extern struct backend be_tftp;
extern struct backend be_ymodem;
extern struct backend be_null;

#endif /* BACKEND_H */
