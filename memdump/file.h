#ifndef FILE_H
#define FILE_H

#include "mystuff.h"

struct serial_if {
    int port;
    void *pvt;
    void (*read) (struct serial_if *, void *, size_t);
    void (*write) (struct serial_if *, const void *, size_t);
};

struct file_info {
    const char *name;
    size_t base;
    size_t size;
    void *pvt;
};


int serial_init(struct serial_if *sif);
void serial_read(struct serial_if *sif, void *data, size_t n);
void serial_write(struct serial_if *sif, const void *data, size_t n);

#endif /* FILE_H */
