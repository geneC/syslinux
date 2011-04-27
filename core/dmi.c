/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Intel Corporation; author: H. Peter Anvin
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
 * Search DMI information for specific data or strings
 */

#include <string.h>
#include <stdio.h>
#include <syslinux/sysappend.h>
#include "core.h"

struct dmi_table {
    uint8_t type;
    uint8_t length;
    uint16_t handle;
};

struct dmi_header {
    char signature[5];
    uint8_t csum;
    uint16_t tbllen;
    uint32_t tbladdr;
    uint16_t nstruc;
    uint8_t revision;
    uint8_t reserved;
};

struct smbios_header {
    char signature[4];
    uint8_t csum;
    uint8_t len;
    uint8_t major;
    uint8_t minor;
    uint16_t maxsize;
    uint8_t revision;
    uint8_t fmt[5];

    struct dmi_header dmi;
};

static const struct dmi_header *dmi;

static uint8_t checksum(const void *buf, size_t len)
{
    const uint8_t *p = buf;
    uint8_t csum = 0;

    while (len--)
	csum += *p++;

    return csum;
}

static bool is_old_dmi(size_t dptr)
{
    const struct dmi_header *dmi = (void *)dptr;

    return !memcmp(dmi->signature, "_DMI_", 5) &&
	!checksum(dmi, 0x0f);
    return false;
}

static bool is_smbios(size_t dptr)
{
    const struct smbios_header *smb = (void *)dptr;

    return !memcmp(smb->signature, "_SM_", 4) &&
	!checksum(smb, smb->len) &&
	is_old_dmi(dptr+16);
}

/*
 * Find the root structure
 */
static void dmi_find_header(void)
{
    size_t dptr;

    /* Search for _SM_ or _DMI_ structure */
    for (dptr = 0xf0000 ; dptr < 0x100000 ; dptr += 16) {
	if (is_smbios(dptr)) {
	    dmi = (const struct dmi_header *)(dptr + 16);
	    break;
	} else if (is_old_dmi(dptr)) {
	    dmi = (const struct dmi_header *)dptr;
	    break;
	}
    }
}

/*
 * Return a specific data element in a specific table, and verify
 * that it is within the bounds of the table.
 */
static const void *dmi_find_data(uint8_t type, uint8_t base, uint8_t length)
{
    const struct dmi_table *table;
    size_t offset, end;
    unsigned int tblcount;

    if (!dmi)
	return NULL;

    if (base < 2)
	return NULL;

    end = base+length;

    offset = 0;
    tblcount = dmi->nstruc;

    while (offset+6 <= dmi->tbllen && tblcount--) {
	table = (const struct dmi_table *)(dmi->tbladdr + offset);

	if (table->type == 127)	/* End of table */
	    break;
	
	if (table->length < sizeof *table)
	    break;		/* Invalid length */

	offset += table->length;

	if (table->type == type && end <= table->length)
	    return (const char *)table + base;

	/* Search for a double NUL terminating the string table */
	while (offset+2 <= dmi->tbllen &&
	       *(const uint16_t *)(dmi->tbladdr + offset) != 0)
	    offset++;

	offset += 2;
    }

    return NULL;
}

/*
 * Return a specific string in a specific table.
 */
static const char *dmi_find_string(uint8_t type, uint8_t base)
{
    const struct dmi_table *table;
    size_t offset;
    unsigned int tblcount;

    if (!dmi)
	return NULL;

    if (base < 2)
	return NULL;

    offset = 0;
    tblcount = dmi->nstruc;

    while (offset+6 <= dmi->tbllen && tblcount--) {
	table = (const struct dmi_table *)(dmi->tbladdr + offset);

	if (table->type == 127)	/* End of table */
	    break;
	
	if (table->length < sizeof *table)
	    break;		/* Invalid length */

	offset += table->length;

	if (table->type == type && base < table->length) {
	    uint8_t index = ((const uint8_t *)table)[base];
	    const char *p = (const char *)table + table->length;
	    const char *str;
	    char c;

	    if (!index)
		return NULL;	/* String not present */

	    while (--index) {
		if (!*p)
		    return NULL;

		do {
		    if (offset++ >= dmi->tbllen)
			return NULL;
		    c = *p++;
		} while (c);
	    }

	    /* Make sure the string is null-terminated */
	    str = p;
	    do {
		if (offset++ >= dmi->tbllen)
		    return NULL;
		c = *p++;
	    } while (c);
	    return str;
	}

	/* Search for a double NUL terminating the string table */
	while (offset+2 <= dmi->tbllen &&
	       *(const uint16_t *)(dmi->tbladdr + offset) != 0)
	    offset++;

	offset += 2;
    }

    return NULL;
}

struct sysappend_dmi_strings {
    const char *prefix;
    enum syslinux_sysappend sa;
    uint8_t index;
    uint8_t offset;
};

static const struct sysappend_dmi_strings dmi_strings[] = {
    { "SYSVENDOR=",  SYSAPPEND_VENDOR,  1, 0x04 },
    { "SYSPRODUCT=", SYSAPPEND_PRODUCT, 1, 0x05 },
    { "SYSVERSION=", SYSAPPEND_VERSION, 1, 0x06 },
    { "SYSSERIAL=",  SYSAPPEND_SERIAL,  1, 0x07 },
    { "SYSSKU=",     SYSAPPEND_SKU,     1, 0x19 },
    { "SYSFAMILY=",  SYSAPPEND_FAMILY,  1, 0x1a },
    { "MBVENDOR=",   SYSAPPEND_VENDOR,  2, 0x04 },
    { "MBPRODUCT=",  SYSAPPEND_PRODUCT, 2, 0x05 },
    { "MBVERSION=",  SYSAPPEND_VERSION, 2, 0x06 },
    { "MBSERIAL=",   SYSAPPEND_SERIAL,  2, 0x07 },
    { "MBASSET=",    SYSAPPEND_ASSET,   2, 0x08 },
    { NULL, 0, 0, 0 }
};

/*
 * Install the string in the string table, if nonempty, after
 * removing leading and trailing whitespace.
 */
static bool is_ctl_or_whitespace(char c)
{
    return (c <= ' ' || c == '\x7f');
}

static const char *dmi_install_string(const char *pfx, const char *str)
{
    const char *p, *ep;
    size_t pfxlen;
    char *nstr, *q;

    if (!str)
	return NULL;

    while (*str && is_ctl_or_whitespace(*str))
	str++;

    if (!*str)
	return NULL;

    ep = p = str;
    while (*p) {
	if (!is_ctl_or_whitespace(*p))
	    ep = p+1;
	p++;
    }

    pfxlen = strlen(pfx);
    q = nstr = malloc(pfxlen + (ep-str) + 1);
    if (!nstr)
	return NULL;
    memcpy(q, pfx, pfxlen);
    q += pfxlen;
    memcpy(q, str, ep-str);
    q += (ep-str);
    *q = '\0';

    return nstr;
}

void dmi_init(void)
{
    const struct sysappend_dmi_strings *ds;

    dmi_find_header();
    if (!dmi)
	return;

    sysappend_set_uuid(dmi_find_data(1, 0x08, 16));

    for (ds = dmi_strings; ds->prefix; ds++) {
	if (!sysappend_strings[ds->sa]) {
	    const char *str = dmi_find_string(ds->index, ds->offset);
	    sysappend_strings[ds->sa] = dmi_install_string(ds->prefix, str);
	}
    }
}
