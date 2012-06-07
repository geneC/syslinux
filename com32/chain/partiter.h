/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2010 H. Peter Anvin - All Rights Reserved
 *   Copyright 2010 Michal Soltys
 *   Copyright 2010 Shao Miller
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

/*
 * partiter.h
 *
 * Provides disk / partition iteration.
 */

#ifndef _COM32_CHAIN_PARTITER_H
#define _COM32_CHAIN_PARTITER_H

#include <stdint.h>
#include <syslinux/disk.h>

#define PI_ERRLOAD 3
#define PI_INSANE 2
#define PI_DONE 1
#define PI_OK 0

struct itertype;
struct part_iter;

struct itertype {
	int (*ctor)(struct part_iter *, va_list *);
	void (*dtor)(struct part_iter *);
	struct part_iter *(*next) (struct part_iter *);
};

#define PI_GPTLABSIZE ((int)sizeof(((struct disk_gpt_part_entry *)0)->name))

struct part_iter {
    const struct itertype *type;
    char *data;
    char *record;
    uint64_t start_lba;
    uint64_t length;
    int index;
    int rawindex;
    struct disk_info di;
    int stepall;
    int status;
    /* internal */
    int index0;
    union _sub {
	struct _dos {
	    uint32_t disk_sig;
	    uint32_t nebr_lba;
	    uint32_t cebr_lba;
	    /* internal */
	    uint32_t ebr_start;
	    uint32_t ebr_size;
	    uint32_t bebr_start;
	    uint32_t bebr_size;
	    int bebr_index0;
	    int skipcnt;
	} dos;
	struct _gpt {
	    struct guid disk_guid;
	    struct guid part_guid;
	    char part_label[PI_GPTLABSIZE/2+1];
	    int pe_count;
	    int pe_size;
	    uint64_t ufirst;
	    uint64_t ulast;
	} gpt;
    } sub;
};

extern const struct itertype * const typedos;
extern const struct itertype * const typegpt;
extern const struct itertype * const typeraw;

struct part_iter *pi_begin(const struct disk_info *, int stepall);
struct part_iter *pi_new(const struct itertype *, ...);
void pi_del(struct part_iter **);
int pi_next(struct part_iter **);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
