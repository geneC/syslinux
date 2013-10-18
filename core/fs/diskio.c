#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <klibc/compiler.h>
#include <core.h>
#include <fs.h>
#include <disk.h>
#include <ilog2.h>
#include <minmax.h>

#include <syslinux/firmware.h>

void getoneblk(struct disk *disk, char *buf, block_t block, int block_size)
{
    int sec_per_block = block_size / disk->sector_size;

    disk->rdwr_sectors(disk, buf, block * sec_per_block, sec_per_block, 0);
}

/*
 * Initialize the device structure.
 */
__export struct device *device_init(void *args)
{
    struct device *dev;

    dev = malloc(sizeof(struct device));
    if (!dev)
        return NULL;

    dev->disk = firmware->disk_init(args);
    if (!dev->disk)
	goto out;

    dev->cache_size = 128*1024;
    dev->cache_data = malloc(dev->cache_size);
    if (!dev->cache_data)
	goto out_disk;
    dev->cache_init = 0;

    return dev;
out_disk:
    free(dev->disk);
out:
    free(dev);
    return NULL;
}
