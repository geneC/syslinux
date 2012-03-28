#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <klibc/compiler.h>
#include <core.h>
#include <fs.h>
#include <disk.h>
#include <ilog2.h>

#include <syslinux/firmware.h>

void getoneblk(struct disk *disk, char *buf, block_t block, int block_size)
{
    int sec_per_block = block_size / disk->sector_size;

    disk->rdwr_sectors(disk, buf, block * sec_per_block, sec_per_block, 0);
}

/*
 * Initialize the device structure.
 *
 * NOTE: the disk cache needs to be revamped to support multiple devices...
 */
struct device * device_init(struct disk_private *args)
{
    static struct device dev;
    static __hugebss char diskcache[128*1024];

    dev.disk = firmware->disk_init(args);
    dev.cache_data = diskcache;
    dev.cache_size = sizeof diskcache;

    return &dev;
}
