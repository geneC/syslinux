/*
 * sys/dirent.h
 */

#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

struct dirent {
    uint32_t d_ino;
    uint32_t d_off;
    uint16_t d_reclen;
    uint16_t d_type;
    char d_name[NAME_MAX + 1];
};

struct file;

typedef struct {
	struct file *dd_dir;
} DIR;

#define DIR_REC_LEN(name) (12 + strlen(name) + 1 + 3) & ~3

#endif /* sys/dirent.h */
