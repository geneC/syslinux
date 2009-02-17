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
	long		d_ino;		/* Inode/File number */
	off_t		d_size;		/* Size of file */
	mode_t		d_mode;		/* Type of file */
	char		d_name[NAME_MAX + 1];
};

typedef struct {
	short		dd_stat;	/* status return from last lookup */
	uint16_t	dd_fd;
	size_t		dd_sect;
	char		dd_name[NAME_MAX + 1];	/* directory */
} DIR;

__extern DIR *opendir(const char *);
__extern struct dirent *readdir(DIR *);
__extern int closedir(DIR *);
__extern DIR *fdopendir(int);

#endif	/* Not _DIRENT_H */
