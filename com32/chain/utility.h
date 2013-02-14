/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *   Copyright 2010 Shao Miller
 *   Copyright 2010-2012 Michal Soltys
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

#ifndef COM32_CHAIN_UTILITY_H
#define COM32_CHAIN_UTILITY_H

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
