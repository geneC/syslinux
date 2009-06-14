#ifndef FAT_FS_H
#define FAT_FS_H

#include <stdint.h>

#define FAT_DIR_ENTRY_SIZE 32
#define DIRENT_SHIFT 5

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN	   0x02
#define FAT_ATTR_SYSTEM	   0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20

#define FAT_MAXFILE	   256

#define FAT_ATTR_LONG_NAME (FAT_ATTR_READ_ONLY             \
                            | FAT_ATTR_HIDDEN              \
                            | FAT_ATTR_SYSTEM              \
                            | FAT_ATTR_VOLUME_ID)

#define FAT_ATTR_VALID	   (FAT_ATTR_READ_ONLY             \
                            | FAT_ATTR_HIDDEN              \
                            | FAT_ATTR_SYSTEM              \
                            | FAT_ATTR_DIRECTORY           \
                            | FAT_ATTR_ARCHIVE)

enum fat_type{ FAT12, FAT16, FAT32 };

/*
 * The fat file system structures 
 */

struct fat_bpb {
        uint8_t  jmp_boot[3];
        uint8_t  oem_name[8];
        uint16_t sector_size;
        uint8_t  bxSecPerClust;
        uint16_t bxResSectors;
        uint8_t  bxFATs;
        uint16_t bxRootDirEnts;
        uint16_t bxSectors;
        uint8_t  media;
        uint16_t bxFATsecs;
        uint16_t sectors_per_track;
        uint16_t num_heads;
        uint32_t num_hidden_sectors;
        uint32_t bsHugeSectors;
       
        union {
                struct {
                        uint8_t  num_ph_drive;
                        uint8_t  reserved;
                        uint8_t  boot_sig;
                        uint32_t num_serial;
                        uint8_t  label[11];
                        uint8_t  fstype[8];
                } __attribute__ ((packed)) fat12_16;

                struct {
                        uint32_t bxFATsecs_32;
                        uint16_t extended_flags;
                        uint16_t fs_version;
                        uint32_t root_cluster;
                        uint16_t fs_info;
                        uint16_t backup_boot_sector;
                        uint8_t  reserved[12];
                        uint8_t  num_ph_drive;
                        uint8_t  reserved1;
                        uint8_t  boot_sig;
                        uint32_t num_serial;
                        uint8_t  label[11];
                        uint8_t  fstype[8];
                } __attribute__ ((packed)) fat32;

        } __attribute__ ((packed)) u;

} __attribute__ ((packed));



struct fat_dir_entry {
        char     name[11];
        uint8_t  attr;
        uint8_t  nt_reserved;
        uint8_t  c_time_tenth;
        uint16_t c_time;
        uint16_t c_date;
        uint16_t a_date;
        uint16_t first_cluster_high;
        uint16_t w_time;
        uint16_t w_date;
        uint16_t first_cluster_low;
        uint32_t file_size;
} __attribute__ ((packed));



struct fat_long_name_entry {
        uint8_t  id;
        uint16_t name1[5];
        uint8_t  attr;
        uint8_t  reserved;
        uint8_t  checksum;
        uint16_t name2[6];
        uint16_t first_cluster;
        uint16_t name3[2];
} __attribute__ ((packed));

__lowmem char syslinux_cfg1[] = "/boot/syslinux/syslinux.cfg";
__lowmem char syslinux_cfg2[] = "/syslinux/syslinux.cfg";
__lowmem char syslinux_cfg3[] = "/syslinux.cfg";
__lowmem char config_name[] = "syslinux.cfg";



#endif /* fat_fs.h */
