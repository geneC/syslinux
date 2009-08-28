/*
 * dirent.h
 */

#ifndef _DIRENT_H
#define _DIRENT_H

#include <klibc/extern.h>
#include <klibc/compiler.h>
#include <stddef.h>
#include <sys/types.h>

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

__extern DIR *opendir(const char *);
__extern struct dirent *readdir(DIR *);
__extern int closedir(DIR *);
__extern DIR *fdopendir(int);

#endif /* Not _DIRENT_H */
