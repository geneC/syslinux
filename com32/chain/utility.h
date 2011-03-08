#ifndef _COM32_CHAIN_UTILITY_H
#define _COM32_CHAIN_UTILITY_H

#include <stdint.h>
#include <syslinux/disk.h>

#define bpbUNK	0
#define bpbV20	1
#define bpbV30	2
#define bpbV32	3
#define bpbV34	4
#define bpbV40	5
#define bpbVNT	6
#define bpbV70	7

#define l2c_cnul 0
#define l2c_cadd 1
#define l2c_cmax 2

void error(const char *msg);
int guid_is0(const struct guid *guid);
void wait_key(void);
void lba2chs(disk_chs *dst, const struct disk_info *di, uint64_t lba, uint32_t mode);
uint32_t get_file_lba(const char *filename);
int drvoff_detect(int type, unsigned int *off);
int bpb_detect(const uint8_t *bpb, const char *tag);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
