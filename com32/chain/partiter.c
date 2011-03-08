/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003-2010 H. Peter Anvin - All Rights Reserved
 *   Copyright 2010 Shao Miller
 *   Copyright 2010 Michal Soltys
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
 * partiter.c
 *
 * Provides disk / partition iteration.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <zlib.h>
#include <syslinux/disk.h>
#include "common.h"
#include "partiter.h"
#include "utility.h"

#define ost_is_ext(type) ((type) == 0x05 || (type) == 0x0F || (type) == 0x85)
#define ost_is_nondata(type) (ost_is_ext(type) || (type) == 0x00)
#define sane(s,l) ((s)+(l) > (s))

/* forwards */

static int iter_ctor(struct part_iter *, va_list *);
static int iter_dos_ctor(struct part_iter *, va_list *);
static int iter_gpt_ctor(struct part_iter *, va_list *);
static void iter_dtor(struct part_iter *);
static struct part_iter *pi_dos_next(struct part_iter *);
static struct part_iter *pi_gpt_next(struct part_iter *);
static struct part_iter *pi_raw_next(struct part_iter *);

static struct itertype types[] = {
   [0] = {
	.ctor = &iter_dos_ctor,
	.dtor = &iter_dtor,
	.next = &pi_dos_next,
}, [1] = {
	.ctor = &iter_gpt_ctor,
	.dtor = &iter_dtor,
	.next = &pi_gpt_next,
}, [2] = {
	.ctor = &iter_ctor,
	.dtor = &iter_dtor,
	.next = &pi_raw_next,
}};

const struct itertype * const typedos = types;
const struct itertype * const typegpt = types+1;
const struct itertype * const typeraw = types+2;

#ifdef DEBUG
static int inv_type(const void *type)
{
    int i, cnt = sizeof(types)/sizeof(types[0]);
    for (i = 0; i < cnt; i++) {
	if (type == types + i)
	    return 0;
    }
    return -1;
}
#endif

/**
 * iter_ctor() - common iterator initialization
 * @iter:	iterator pointer
 * @args(0):	disk_info structure used for disk functions
 * @args(1):	stepall modifier
 *
 * Second and further arguments are passed as a pointer to va_list
 **/
static int iter_ctor(struct part_iter *iter, va_list *args)
{
    const struct disk_info *di = va_arg(*args, const struct disk_info *);
    int stepall = va_arg(*args, int);

#ifdef DEBUG
    if (!di)
	return -1;
#endif

    memcpy(&iter->di, di, sizeof(struct disk_info));
    iter->stepall = stepall;
    iter->index0 = -1;
    iter->length = di->lbacnt;

    return 0;
}

/**
 * iter_dtor() - common iterator cleanup
 * @iter:	iterator pointer
 *
 **/
static void iter_dtor(struct part_iter *iter)
{
    free(iter->data);
}

/**
 * iter_dos_ctor() - MBR/EBR iterator specific initialization
 * @iter:	iterator pointer
 * @args(0):	disk_info structure used for disk functions
 * @args(1):	pointer to buffer with loaded valid MBR
 *
 * Second and further arguments are passed as a pointer to va_list.
 * This function only makes rudimentary checks. If user uses
 * pi_new(), he/she is responsible for doing proper sanity checks.
 **/
static int iter_dos_ctor(struct part_iter *iter, va_list *args)
{
    const struct disk_dos_mbr *mbr;

    /* uses args(0) */
    if (iter_ctor(iter, args))
	return -1;

    mbr = va_arg(*args, const struct disk_dos_mbr *);

#ifdef DEBUG
    if (!mbr)
	goto bail;
#endif

    if (!(iter->data = malloc(sizeof(struct disk_dos_mbr))))
	goto bail;

    memcpy(iter->data, mbr, sizeof(struct disk_dos_mbr));

    iter->sub.dos.bebr_index0 = -1;
    iter->sub.dos.disk_sig = mbr->disk_sig;

    return 0;
bail:
    iter->type->dtor(iter);
    return -1;
}

/**
 * iter_gpt_ctor() - GPT iterator specific initialization
 * @iter:	iterator pointer
 * @args(0):	ptr to disk_info structure
 * @args(1):	ptr to buffer with GPT header
 * @args(2):	ptr to buffer with GPT partition list
 *
 * Second and further arguments are passed as a pointer to va_list.
 * This function only makes rudimentary checks. If user uses
 * pi_new(), he/she is responsible for doing proper sanity checks.
 **/
static int iter_gpt_ctor(struct part_iter *iter, va_list *args)
{
    uint64_t siz;
    const struct disk_gpt_header *gpth;
    const struct disk_gpt_part_entry *gptl;

    /* uses args(0) */
    if (iter_ctor(iter, args))
	return -1;

    gpth = va_arg(*args, const struct disk_gpt_header *);
    gptl = va_arg(*args, const struct disk_gpt_part_entry *);

#ifdef DEBUG
    if (!gpth || !gptl)
	goto bail;
#endif

    siz = (uint64_t)gpth->part_count * gpth->part_size;

#ifdef DEBUG
    if (!siz || (siz + iter->di.bps - 1) / iter->di.bps > 255u ||
	    gpth->part_size < sizeof(struct disk_gpt_part_entry)) {
	goto bail;
    }
#endif

    if (!(iter->data = malloc((size_t)siz)))
	goto bail;

    memcpy(iter->data, gptl, (size_t)siz);

    iter->sub.gpt.pe_count = (int)gpth->part_count;
    iter->sub.gpt.pe_size = (int)gpth->part_size;
    iter->sub.gpt.ufirst = gpth->lba_first_usable;
    iter->sub.gpt.ulast = gpth->lba_last_usable;

    memcpy(&iter->sub.gpt.disk_guid, &gpth->disk_guid, sizeof(struct guid));

    return 0;
bail:
    iter->type->dtor(iter);
    return -1;
}

/* Logical partition must be sane, meaning:
 * - must be data or empty
 * - must have non-0 start and length
 * - values must not wrap around 32bit
 * - must be inside current EBR frame
 */

static int notsane_logical(const struct part_iter *iter)
{
    const struct disk_dos_part_entry *dp;
    uint32_t end_log;

    dp = ((struct disk_dos_mbr *)iter->data)->table;

    if (!dp[0].ostype)
	return 0;

    if (ost_is_ext(dp[0].ostype)) {
	error("1st EBR entry must be data or empty.\n");
	return -1;
    }

    end_log = dp[0].start_lba + dp[0].length;

    if (!dp[0].start_lba ||
	!dp[0].length ||
	!sane(dp[0].start_lba, dp[0].length) ||
	end_log > iter->sub.dos.ebr_size) {

	error("Insane logical partition.\n");
	return -1;
    }

    return 0;
}

/* Extended partition must be sane, meaning:
 * - must be extended or empty
 * - must have non-0 start and length
 * - values must not wrap around 32bit
 * - must be inside base EBR frame
 */

static int notsane_extended(const struct part_iter *iter)
{
    const struct disk_dos_part_entry *dp;
    uint32_t end_ebr;

    dp = ((struct disk_dos_mbr *)iter->data)->table;

    if (!dp[1].ostype)
	return 0;

    if (!ost_is_nondata(dp[1].ostype)) {
	error("2nd EBR entry must be extended or empty.\n");
	return -1;
    }

    end_ebr = dp[1].start_lba + dp[1].length;

    if (!dp[1].start_lba ||
	!dp[1].length ||
	!sane(dp[1].start_lba, dp[1].length) ||
	end_ebr > iter->sub.dos.bebr_size) {

	error("Insane extended partition.\n");
	return -1;
    }

    return 0;
}

/* Primary partition must be sane, meaning:
 * - must have non-0 start and length
 * - values must not wrap around 32bit
 */

static int notsane_primary(const struct part_iter *iter)
{
    const struct disk_dos_part_entry *dp;
    dp = ((struct disk_dos_mbr *)iter->data)->table + iter->index0;

    if (!dp->ostype)
	return 0;

    if (!dp->start_lba ||
	!dp->length ||
	!sane(dp->start_lba, dp->length) ||
	dp->start_lba + dp->length > iter->di.lbacnt) {
	error("Insane primary (MBR) partition.\n");
	return -1;
    }

    return 0;
}

static int notsane_gpt(const struct part_iter *iter)
{
    const struct disk_gpt_part_entry *gp;
    gp = (const struct disk_gpt_part_entry *)
	(iter->data + iter->index0 * iter->sub.gpt.pe_size);

    if (guid_is0(&gp->type))
	return 0;

    if (gp->lba_first < iter->sub.gpt.ufirst ||
	gp->lba_last > iter->sub.gpt.ulast) {
	error("Insane GPT partition.\n");
	return -1;
    }

    return 0;
}

static int pi_dos_next_mbr(struct part_iter *iter, uint32_t *lba,
			    struct disk_dos_part_entry **_dp)
{
    struct disk_dos_part_entry *dp;

    while (++iter->index0 < 4) {
	dp = ((struct disk_dos_mbr *)iter->data)->table + iter->index0;

	if (notsane_primary(iter)) {
	    iter->status = PI_INSANE;
	    goto bail;
	}

	if (ost_is_ext(dp->ostype)) {
	    if (iter->sub.dos.bebr_index0 >= 0) {
		error("You have more than 1 extended partition.\n");
		iter->status = PI_INSANE;
		goto bail;
	    }
	    /* record base EBR index */
	    iter->sub.dos.bebr_index0 = iter->index0;
	}
	if (!ost_is_nondata(dp->ostype) || iter->stepall) {
	    *lba = dp->start_lba;
	    *_dp = dp;
	    break;
	}
    }

    return 0;
bail:
    return -1;
}

static int prep_base_ebr(struct part_iter *iter)
{
    struct disk_dos_part_entry *dp;

    if (iter->sub.dos.bebr_index0 < 0)	/* if we don't have base extended partition at all */
	return -1;
    else if (!iter->sub.dos.bebr_start) { /* if not initialized yet */
	dp = ((struct disk_dos_mbr *)iter->data)->table + iter->sub.dos.bebr_index0;

	iter->sub.dos.bebr_start = dp->start_lba;
	iter->sub.dos.bebr_size = dp->length;

	iter->sub.dos.ebr_start = 0;
	iter->sub.dos.ebr_size = iter->sub.dos.bebr_size;

	iter->sub.dos.cebr_lba = 0;
	iter->sub.dos.nebr_lba = iter->sub.dos.bebr_start;

	iter->index0--;
    }
    return 0;
}

static int pi_dos_next_ebr(struct part_iter *iter, uint32_t *lba,
			    struct disk_dos_part_entry **_dp)
{
    struct disk_dos_part_entry *dp;

    if (prep_base_ebr(iter)) {
	iter->status = PI_DONE;
	return -1;
    }

    while (++iter->index0 < 1024 && iter->sub.dos.nebr_lba) {
	free(iter->data);
	if (!(iter->data =
		    disk_read_sectors(&iter->di, iter->sub.dos.nebr_lba, 1))) {
	    error("Couldn't load EBR.\n");
	    iter->status = PI_ERRLOAD;
	    return -1;
	}

	if (notsane_logical(iter) || notsane_extended(iter)) {
	    iter->status = PI_INSANE;
	    return -1;
	}

	dp = ((struct disk_dos_mbr *)iter->data)->table;

	iter->sub.dos.cebr_lba = iter->sub.dos.nebr_lba;

	/* setup next frame values */
	if (dp[1].ostype) {
	    iter->sub.dos.ebr_start = dp[1].start_lba;
	    iter->sub.dos.ebr_size = dp[1].length;
	    iter->sub.dos.nebr_lba = iter->sub.dos.bebr_start + dp[1].start_lba;
	} else {
	    iter->sub.dos.ebr_start = 0;
	    iter->sub.dos.ebr_size = 0;
	    iter->sub.dos.nebr_lba = 0;
	}

	if (!dp[0].ostype)
	    iter->sub.dos.skipcnt++;

	if (dp[0].ostype || iter->stepall) {
	    *lba = iter->sub.dos.cebr_lba + dp[0].start_lba;
	    *_dp = dp;
	    return 0;
	}
	/*
	 * This way it's possible to continue, if some crazy soft left a "hole"
	 * - EBR with a valid extended partition without a logical one. In
	 * such case, linux will not reserve a number for such hole - so we
	 * don't increase index0. If stepall flag is set, we will never reach
	 * this place.
	 */
    }
    iter->status = PI_DONE;
    return -1;
}

static struct part_iter *pi_dos_next(struct part_iter *iter)
{
    uint32_t start_lba = 0;
    struct disk_dos_part_entry *dos_part = NULL;

    if (iter->status)
	goto bail;

    /* look for primary partitions */
    if (iter->index0 < 4 &&
	    pi_dos_next_mbr(iter, &start_lba, &dos_part))
	goto bail;

    /* look for logical partitions */
    if (iter->index0 >= 4 &&
	    pi_dos_next_ebr(iter, &start_lba, &dos_part))
	goto bail;

    /*
     * note special index handling, if we have stepall set -
     * this is made to keep index consistent with non-stepall
     * iterators
     */

    if (iter->index0 >= 4 && !dos_part->ostype)
	iter->index = -1;
    else
	iter->index = iter->index0 - iter->sub.dos.skipcnt + 1;
    iter->rawindex = iter->index0 + 1;
    iter->start_lba = start_lba;
    iter->length = dos_part->length;
    iter->record = (char *)dos_part;

#ifdef DEBUG
    disk_dos_part_dump(dos_part);
#endif

    return iter;
bail:
    return NULL;
}

static void gpt_conv_label(struct part_iter *iter)
{
    const struct disk_gpt_part_entry *gp;
    const int16_t *orig_lab;

    gp = (const struct disk_gpt_part_entry *)
	(iter->data + iter->index0 * iter->sub.gpt.pe_size);
    orig_lab = (const int16_t *)gp->name;

    /* caveat: this is very crude conversion */
    for (int i = 0; i < PI_GPTLABSIZE/2; i++) {
	iter->sub.gpt.part_label[i] = (char)orig_lab[i];
    }
    iter->sub.gpt.part_label[PI_GPTLABSIZE/2] = 0;
}

static struct part_iter *pi_gpt_next(struct part_iter *iter)
{
    const struct disk_gpt_part_entry *gpt_part = NULL;

    if (iter->status)
	goto bail;

    while (++iter->index0 < iter->sub.gpt.pe_count) {
	gpt_part = (const struct disk_gpt_part_entry *)
	    (iter->data + iter->index0 * iter->sub.gpt.pe_size);

	if (notsane_gpt(iter)) {
	    iter->status = PI_INSANE;
	    goto bail;
	}

	if (!guid_is0(&gpt_part->type) || iter->stepall)
	    break;
    }
    /* no more partitions ? */
    if (iter->index0 == iter->sub.gpt.pe_count) {
	iter->status = PI_DONE;
	goto bail;
    }
    /* gpt_part is guaranteed to be valid here */
    iter->index = iter->index0 + 1;
    iter->rawindex = iter->index0 + 1;
    iter->start_lba = gpt_part->lba_first;
    iter->length = gpt_part->lba_last - gpt_part->lba_first + 1;
    iter->record = (char *)gpt_part;
    memcpy(&iter->sub.gpt.part_guid, &gpt_part->uid, sizeof(struct guid));
    gpt_conv_label(iter);

#ifdef DEBUG
    disk_gpt_part_dump(gpt_part);
#endif

    return iter;
bail:
    return NULL;
}

static struct part_iter *pi_raw_next(struct part_iter *iter)
{
    iter->status = PI_DONE;
    return NULL;
}

static int check_crc(uint32_t crc_match, const uint8_t *buf, unsigned int siz)
{
    uint32_t crc;

    crc = crc32(0, NULL, 0);
    crc = crc32(crc, buf, siz);

    return crc_match != crc;
}

static int gpt_check_hdr_crc(const struct disk_info * const diskinfo, struct disk_gpt_header **_gh)
{
    struct disk_gpt_header *gh = *_gh;
    uint64_t lba_alt;
    uint32_t hold_crc32;

    hold_crc32 = gh->chksum;
    gh->chksum = 0;
    if (check_crc(hold_crc32, (const uint8_t *)gh, gh->hdr_size)) {
	error("WARNING: Primary GPT header checksum invalid.\n");
	/* retry with backup */
	lba_alt = gh->lba_alt;
	free(gh);
	if (!(gh = *_gh = disk_read_sectors(diskinfo, lba_alt, 1))) {
	    error("Couldn't read backup GPT header.\n");
	    return -1;
	}
	hold_crc32 = gh->chksum;
	gh->chksum = 0;
	if (check_crc(hold_crc32, (const uint8_t *)gh, gh->hdr_size)) {
	    error("Secondary GPT header checksum invalid.\n");
	    return -1;
	}
    }
    /* restore old checksum */
    gh->chksum = hold_crc32;

    return 0;
}

/*
 * ----------------------------------------------------------------------------
 * Following functions are for users to call.
 * ----------------------------------------------------------------------------
 */


int pi_next(struct part_iter **_iter)
{
    struct part_iter *iter;

    if(!_iter || !*_iter)
	return 0;
    iter = *_iter;
#ifdef DEBUG
    if (inv_type(iter->type)) {
	error("This is not a valid iterator.\n");
	return 0;
    }
#endif
    if ((iter = iter->type->next(iter))) {
	*_iter = iter;
    }
    return (*_iter)->status;
}

/**
 * pi_new() - get new iterator
 * @itertype:	iterator type
 * @...:	variable arguments passed to ctors
 *
 * Variable arguments depend on the type. Please see functions:
 * iter_gpt_ctor() and iter_dos_ctor() for details.
 **/
struct part_iter *pi_new(const struct itertype *type, ...)
{
    int badctor = 0;
    struct part_iter *iter = NULL;
    va_list ap;

    va_start(ap, type);

#ifdef DEBUG
    if (inv_type(type)) {
	error("Unknown iterator requested.\n");
	goto bail;
    }
#endif

    if (!(iter = malloc(sizeof(struct part_iter)))) {
	error("Couldn't allocate memory for the iterator.\n");
	goto bail;
    }

    memset(iter, 0, sizeof(struct part_iter));
    iter->type = type;

    if (type->ctor(iter, &ap)) {
	badctor = -1;
	error("Cannot initialize the iterator.\n");
	goto bail;
    }

bail:
    va_end(ap);
    if (badctor) {
	free(iter);
	iter = NULL;
    }
    return iter;
}

/**
 * pi_del() - delete iterator
 * @iter:       iterator double pointer
 *
 **/

void pi_del(struct part_iter **_iter)
{
    struct part_iter *iter;

    if(!_iter || !*_iter)
	return;
    iter = *_iter;

#ifdef DEBUG
    if (inv_type(iter->type)) {
	error("This is not a valid iterator.\n");
	return;
    }
#endif

    iter->type->dtor(iter);
    free(iter);
    *_iter = NULL;
}

/**
 * pi_begin() - check disk, validate, and get proper iterator
 * @di:	    diskinfo struct pointer
 *
 * This function checks the disk for GPT or legacy partition table and allocates
 * an appropriate iterator.
 **/
struct part_iter *pi_begin(const struct disk_info *di, int stepall)
{
    int setraw = 0;
    struct part_iter *iter = NULL;
    struct disk_dos_mbr *mbr = NULL;
    struct disk_gpt_header *gpth = NULL;
    struct disk_gpt_part_entry *gptl = NULL;

    /* Read MBR */
    if (!(mbr = disk_read_sectors(di, 0, 1))) {
	error("Couldn't read first disk sector.\n");
	goto bail;
    }

    setraw = -1;

    /* Check for MBR magic*/
    if (mbr->sig != disk_mbr_sig_magic) {
	error("No MBR magic.\n");
	goto bail;
    }

    /* Check for GPT protective MBR */
    if (mbr->table[0].ostype == 0xEE) {
	if (!(gpth = disk_read_sectors(di, 1, 1))) {
	    error("Couldn't read potential GPT header.\n");
	    goto bail;
	}
    }

    if (gpth && gpth->rev.uint32 == 0x00010000 &&
	    !memcmp(gpth->sig, disk_gpt_sig_magic, sizeof(disk_gpt_sig_magic))) {
	/* looks like GPT v1.0 */
	uint64_t gpt_loff;	    /* offset to GPT partition list in sectors */
	uint64_t gpt_lsiz;	    /* size of GPT partition list in bytes */
	uint64_t gpt_lcnt;	    /* size of GPT partition in sectors */
#ifdef DEBUG
	puts("Looks like a GPT v1.0 disk.");
	disk_gpt_header_dump(gpth);
#endif
	/* Verify checksum, fallback to backup, then bail if invalid */
	if (gpt_check_hdr_crc(di, &gpth))
	    goto bail;

	gpt_loff = gpth->lba_table;
	gpt_lsiz = (uint64_t)gpth->part_size * gpth->part_count;
	gpt_lcnt = (gpt_lsiz + di->bps - 1) / di->bps;

	/*
	 * disk_read_sectors allows reading of max 255 sectors, so we use
	 * it as a sanity check base. EFI doesn't specify max (AFAIK).
	 * Apart from that, some extensive sanity checks.
	 */
	if (!gpt_loff || !gpt_lsiz || gpt_lcnt > 255u ||
		gpth->lba_first_usable > gpth->lba_last_usable ||
		!sane(gpt_loff, gpt_lcnt) ||
		gpt_loff + gpt_lcnt > gpth->lba_first_usable ||
		!sane(gpth->lba_last_usable, gpt_lcnt) ||
		gpth->lba_last_usable + gpt_lcnt >= gpth->lba_alt ||
		gpth->lba_alt >= di->lbacnt ||
		gpth->part_size < sizeof(struct disk_gpt_part_entry)) {
	    error("Invalid GPT header's values.\n");
	    goto bail;
	}
	if (!(gptl = disk_read_sectors(di, gpt_loff, (uint8_t)gpt_lcnt))) {
	    error("Couldn't read GPT partition list.\n");
	    goto bail;
	}
	/* Check array checksum(s). */
	if (check_crc(gpth->table_chksum, (const uint8_t *)gptl, (unsigned int)gpt_lsiz)) {
	    error("WARNING: GPT partition list checksum invalid, trying backup.\n");
	    free(gptl);
	    /* secondary array directly precedes secondary header */
	    if (!(gptl = disk_read_sectors(di, gpth->lba_alt - gpt_lcnt, (uint8_t)gpt_lcnt))) {
		error("Couldn't read backup GPT partition list.\n");
		goto bail;
	    }
	    if (check_crc(gpth->table_chksum, (const uint8_t *)gptl, (unsigned int)gpt_lsiz)) {
		error("Backup GPT partition list checksum invalid.\n");
		goto bail;
	    }
	}
	/* allocate iterator and exit */
	iter = pi_new(typegpt, di, stepall, gpth, gptl);
    } else {
	/* looks like MBR */
	iter = pi_new(typedos, di, stepall, mbr);
    }

    setraw = 0;
bail:
    if (setraw) {
	error("WARNING: treating disk as raw.\n");
	iter = pi_new(typeraw, di, stepall);
    }
    free(mbr);
    free(gpth);
    free(gptl);

    return iter;
}

/* vim: set ts=8 sts=4 sw=4 noet: */
