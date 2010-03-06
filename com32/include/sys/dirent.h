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

enum dirent_types {
    DT_UNKNOWN	=  0,
    DT_FIFO	=  1,
    DT_CHR	=  2,
    DT_DIR	=  4,
    DT_BLK	=  6,
    DT_REG	=  8,
    DT_LNK	= 10,
    DT_SOCK	= 12,
    DT_WHT	= 14,
};

struct _DIR_;
typedef struct _DIR_ DIR;

#endif /* sys/dirent.h */
