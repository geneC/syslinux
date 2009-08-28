#ifndef DIR_H
#define DIR_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <com32.h>
#include "disk.h"
#include "fs.h"

struct dirent {
	uint32_t d_ino;
	uint32_t d_off;
	uint16_t d_reclen;
	uint16_t d_type;
	char d_name[256];
};

struct file;

typedef struct {
	struct file *dd_dir;
} DIR;

#define DIR_REC_LEN(name) (12 + strlen(name) + 1 + 3) & ~3


#endif /* dir.h */
