#include <com32.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <syslinux/disk.h>
#include "utility.h"

void error(const char *msg)
{
    fputs(msg, stderr);
}

int guid_is0(const struct guid *guid)
{
    return !*(const uint64_t *)guid && !*((const uint64_t *)guid+1);
}

void wait_key(void)
{
    int cnt;
    char junk;

    /* drain */
    do {
	errno = 0;
	cnt = read(0, &junk, 1);
    } while (cnt > 0 || (cnt < 0 && errno == EAGAIN));

    /* wait */
    do {
	errno = 0;
	cnt = read(0, &junk, 1);
    } while (!cnt || (cnt < 0 && errno == EAGAIN));
}

uint32_t lba2chs(const struct disk_info *di, uint64_t lba)
{
    uint32_t c, h, s, t;

    if (di->cbios) {
	if (lba >= di->cyl * di->head * di->sect) {
	    s = di->sect;
	    h = di->head - 1;
	    c = di->cyl - 1;
	    goto out;
	}
	s = ((uint32_t)lba % di->sect) + 1;
	t = (uint32_t)lba / di->sect;
	h = t % di->head;
	c = t / di->head;
    } else
	goto fallback;

out:
    return h | (s << 8) | ((c & 0x300) << 6) | ((c & 0xFF) << 16);

fallback:
    if (di->disk & 0x80)
	return 0x00FFFFFE; /* 1023/63/254 */
    else
	/*
	 * adjust ?
	 * this is somewhat "useful" with partitioned floppy,
	 * maybe stick to 2.88mb ?
	 */
	return 0x004F1201; /* 79/18/1 */
#if 0
	return 0x004F2401; /* 79/36/1 */
#endif
}

uint32_t get_file_lba(const char *filename)
{
    com32sys_t inregs;
    uint32_t lba;

    /* Start with clean registers */
    memset(&inregs, 0, sizeof(com32sys_t));

    /* Put the filename in the bounce buffer */
    strlcpy(__com32.cs_bounce, filename, __com32.cs_bounce_size);

    /* Call comapi_open() which returns a structure pointer in SI
     * to a structure whose first member happens to be the LBA.
     */
    inregs.eax.w[0] = 0x0006;
    inregs.esi.w[0] = OFFS(__com32.cs_bounce);
    inregs.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inregs, &inregs);

    if ((inregs.eflags.l & EFLAGS_CF) || inregs.esi.w[0] == 0) {
	return 0;		/* Filename not found */
    }

    /* Since the first member is the LBA, we simply cast */
    lba = *((uint32_t *) MK_PTR(inregs.ds, inregs.esi.w[0]));

    /* Clean the registers for the next call */
    memset(&inregs, 0, sizeof(com32sys_t));

    /* Put the filename in the bounce buffer */
    strlcpy(__com32.cs_bounce, filename, __com32.cs_bounce_size);

    /* Call comapi_close() to free the structure */
    inregs.eax.w[0] = 0x0008;
    inregs.esi.w[0] = OFFS(__com32.cs_bounce);
    inregs.es = SEG(__com32.cs_bounce);
    __com32.cs_intcall(0x22, &inregs, &inregs);

    return lba;
}

/* drive offset detection */
int drvoff_detect(int type, unsigned int *off)
{
    if (bpbV40 <= type && type <= bpbVNT) {
	*off = 0x24;
    } else if (type == bpbV70) {
	*off = 0x40;
    } else
	return 0;

    return -1;
}

/*
 * heuristics could certainly be improved
 */
int bpb_detect(const uint8_t *sec)
{
    int a, b, c, jmp = -1, rev = -1;

    /* media descriptor check */
    if ((sec[0x15] & 0xF0) != 0xF0)
	return -1;

    if (sec[0] == 0xEB)	/* jump short */
	jmp = 2 + *(int8_t *)(sec + 1);
    else if (sec[0] == 0xE9) /* jump near */
	jmp = 3 + *(int16_t *)(sec + 1);

    if (jmp < 0)    /* no boot code at all ? */
	goto nocode;

    /* sanity */
    if (jmp < 0x18 || jmp > 0x1F0)
	return -1;

    /* detect by jump */
    if (jmp >= 0x18 && jmp < 0x1E)
	rev = bpbV20;
    else if (jmp >= 0x1E && jmp < 0x20)
	rev = bpbV30;
    else if (jmp >= 0x20 && jmp < 0x24)
	rev = bpbV32;
    else if (jmp >= 0x24 && jmp < 0x46)
	rev = bpbV34;

    /* TODO: some better V2 - V3.4 checks ? */

    if (rev >= 0)
	return rev;

nocode:
    a = memcmp(sec + 0x03, "NTFS", 4);
    b = memcmp(sec + 0x36, "FAT", 3);
    c = memcmp(sec + 0x52, "FAT", 3);	/* ext. DOS 7+ bs */

    if ((sec[0x26] & 0xFE) == 0x28 && !b) {
	rev = bpbV40;
    } else if (sec[0x26] == 0x80 && !a) {
	rev = bpbVNT;
    } else if ((sec[0x42] & 0xFE) == 0x28 && !c) {
	rev = bpbV70;
    }

    return rev;
}


/* vim: set ts=8 sts=4 sw=4 noet: */
