#ifndef BACKEND_H
#define BACKEND_H

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <zlib.h>

struct backend {
    const char *name;
    int blocksize;

    int (*open)(struct backend *, const char *argv[]);
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
    };
};

/* zout.c */
int init_data(struct backend *be, const char *argv[]);
int write_data(struct backend *be, const void *buf, size_t len, bool flush);

/* cpio.c */
#define cpio_init init_data
int cpio_mkdir(struct backend *be, const char *filename);
int cpio_writefile(struct backend *be, const char *filename,
		   const void *data, size_t len);
int cpio_close(struct backend *be);

/* backends.c */
struct backend *get_backend(const char *name);

#endif /* BACKEND_H */
