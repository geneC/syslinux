#ifndef _COM32_CHAIN_UTILITY_H
#define _COM32_CHAIN_UTILITY_H

#include <stdint.h>
#include <syslinux/disk.h>

void error(const char *msg);
int guid_is0(const struct guid *guid);
void wait_key(void);
uint32_t lba2chs(const struct disk_info *di, uint64_t lba);
uint32_t get_file_lba(const char *filename);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
