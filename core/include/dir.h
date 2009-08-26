#ifndef DIR_H
#define DIR_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <com32.h>
#include "disk.h"

struct dirent {
	uint32_t d_ino;
	uint32_t d_off;
	uint16_t d_reclen;
	uint16_t d_type;
	char d_name[256];
};

typedef struct {
	uint16_t dd_stat;
	uint16_t dd_sect;
	sector_t dd_offset;
	char dd_name[256];
} DIR;

#define DIR_REC_LEN(name) (12 + strlen(name) + 1 + 3) & ~3

/*
 * funtions 
 */
int fill_dir(struct dirent *);

#endif /* dir.h */
