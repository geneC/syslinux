/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * Dump ACPI information
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sysdump.h"
#include "backend.h"

struct acpi_rsdp {
    uint8_t  magic[8];		/* "RSD PTR " */
    uint8_t  csum;
    char     oemid[6];
    uint8_t  rev;
    uint32_t rsdt_addr;
    uint32_t len;
    uint64_t xdst_addr;
    uint8_t  xcsum;
    uint8_t  rsvd[3];
};

struct acpi_hdr {
    char     sig[4];		/* Signature */
    uint32_t len;
    uint8_t  rev;
    uint8_t  csum;
    char     oemid[6];
    char     oemtblid[16];
    uint32_t oemrev;
    uint32_t creatorid;
    uint32_t creatorrev;
};

struct acpi_rsdt {
    struct acpi_hdr hdr;
    uint32_t entry[0];
};

enum tbl_errs {
    ERR_NONE,			/* No errors */
    ERR_CSUM,			/* Invalid checksum */
    ERR_SIZE,			/* Impossibly large table */
    ERR_NOSIG			/* No signature */
};

static uint8_t checksum_range(const void *start, uint32_t size)
{
    const uint8_t *p = start;
    uint8_t csum = 0;

    while (size--)
	csum += *p++;

    return csum;
}

static enum tbl_errs is_valid_table(const void *ptr)
{
    const struct acpi_hdr *hdr = ptr;

    if (hdr->sig[0] == 0)
	return ERR_NOSIG;

    if (hdr->len < 10 || hdr->len > (1 << 20)) {
	/* Either insane or too large to dump */
	return ERR_SIZE;
    }

    return checksum_range(hdr, hdr->len) == 0 ? ERR_NONE : ERR_CSUM;
}

static const struct acpi_rsdp *scan_for_rsdp(uint32_t base, uint32_t end)
{
    for (base &= ~15; base < end-20; base += 16) {
	const struct acpi_rsdp *rsdp = (const struct acpi_rsdp *)base;

	if (memcmp(rsdp->magic, "RSD PTR ", 8))
	    continue;

	if (checksum_range(rsdp, 20))
	    continue;

	if (rsdp->rev > 0) {
	    if (base + rsdp->len >= end ||
		checksum_range(rsdp, rsdp->len))
		continue;
	}

	return rsdp;
    }

    return NULL;
}

static const struct acpi_rsdp *find_rsdp(void)
{
    uint32_t ebda;
    const struct acpi_rsdp *rsdp;

    ebda = (*(uint16_t *)0x40e) << 4;
    if (ebda >= 0x70000 && ebda < 0xa0000) {
	rsdp = scan_for_rsdp(ebda, ebda+1024);

	if (rsdp)
	    return rsdp;
    }

    return scan_for_rsdp(0xe0000, 0x100000);
}

static void dump_table(struct backend *be,
		       const char name[], const void *ptr, uint32_t len)
{
    char namebuf[64];

    /* XXX: this make cause the same directory to show up more than once */
    snprintf(namebuf, sizeof namebuf, "acpi/%4.4s", name);
    cpio_mkdir(be, namebuf);

    snprintf(namebuf, sizeof namebuf, "acpi/%4.4s/%08x", name, (uint32_t)ptr);
    cpio_hdr(be, MODE_FILE, len, namebuf);

    write_data(be, ptr, len);
}

void dump_acpi(struct backend *be)
{
    const struct acpi_rsdp *rsdp;
    const struct acpi_rsdt *rsdt;
    uint32_t rsdp_len;
    uint32_t i, n;

    rsdp = find_rsdp();

    if (!rsdp)
	return;			/* No ACPI information found */

    cpio_mkdir(be, "acpi");

    rsdp_len = rsdp->rev > 0 ? rsdp->len : 20;

    dump_table(be, "RSDP", rsdp, rsdp_len);

    rsdt = (const struct acpi_rsdt *)rsdp->rsdt_addr;

    if (memcmp(rsdt->hdr.sig, "RSDT", 4) || is_valid_table(rsdt) != ERR_NONE)
	return;

    dump_table(be, rsdt->hdr.sig, rsdt, rsdt->hdr.len);

    if (rsdt->hdr.len < 36)
	return;

    n = (rsdt->hdr.len - 36) >> 2;

    for (i = 0; i < n; i++) {
	const struct acpi_hdr *hdr = (const struct acpi_hdr *)(rsdt->entry[i]);

	if (is_valid_table(hdr) <= ERR_CSUM)
	    dump_table(be, hdr->sig, hdr, hdr->len);
    }
}
