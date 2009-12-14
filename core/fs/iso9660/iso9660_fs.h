#ifndef ISO9660_FS_H
#define ISO9660_FS_H

#include <stdint.h>

/* The root dir entry offset in the primary volume descriptor */
#define ROOT_DIR_OFFSET   156

struct iso_dir_entry {
        uint8_t length;                         /* 00 */
        uint8_t ext_attr_length;                /* 01 */    
        uint8_t extent[8];                      /* 02 */    
        uint8_t size[8];                        /* 0a */  
        uint8_t date[7];                        /* 12 */
        uint8_t flags;                          /* 19 */
        uint8_t file_unit_size;                 /* 1a */
        uint8_t interleave;                     /* 1b */
        uint8_t volume_sequence_number[4];      /* 1c */
        uint8_t name_len;                       /* 20 */
        char    name[0];                        /* 21 */
};

struct iso_sb_info {
	struct iso_dir_entry root;
};

#endif /* iso9660_fs.h */
