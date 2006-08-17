/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2005 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * ethersel.c
 *
 * Search for an Ethernet card with a known PCI signature, and run
 * the corresponding Ethernet module.
 *
 * To use this, set up a syslinux config file like this:
 *
 * PROMPT 0
 * DEFAULT ethersel.c32
 * # DEV [DID xxxx:yyyy[/mask]] [RID zz-zz] [SID uuuu:vvvv[/mask]] commandline
 * # ...
 *
 * DID = PCI device ID
 * RID = Revision ID (range)
 * SID = Subsystem ID
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <sys/pci.h>
#include <com32.h>

#ifdef DEBUG
# define dprintf printf
#else
# define dprintf(...) ((void)0)
#endif

struct match {
  struct match *next;
  uint32_t did;
  uint32_t did_mask;
  uint32_t sid;
  uint32_t sid_mask;
  uint8_t rid_min, rid_max;
  char *filename;
};

static const char *
get_config(void)
{
  static com32sys_t r;

  r.eax.w[0] = 0x000E;
  __intcall(0x22, &r, &r);

  return MK_PTR(r.es, r.ebx.w[0]);
}

static char *
skipspace(char *p)
{
  while ( *p && *p <= ' ' )
    p++;

  return p;
}

#define MAX_LINE 512

/* Check to see if we are at a certain keyword (case insensitive) */
static int looking_at(const char *line, const char *kwd)
{
  const char *p = line;
  const char *q = kwd;

  while ( *p && *q && ((*p^*q) & ~0x20) == 0 ) {
    p++;
    q++;
  }

  if ( *q )
    return 0;                   /* Didn't see the keyword */

  return *p <= ' ';             /* Must be EOL or whitespace */
}

static char *
get_did(char *p, uint32_t *idptr, uint32_t *maskptr)
{
  unsigned long vid, did, m1, m2;

  *idptr   = -1;
  *maskptr = 0xffffffff;

  vid = strtoul(p, &p, 16);
  if ( *p != ':' )
    return p;			/* Bogus ID */
  did = strtoul(p+1, &p, 16);

  *idptr = (did << 16) + vid;

  if ( *p == '/' ) {
    m1 = strtoul(p+1, &p, 16);
    if ( *p != ':' ) {
      *maskptr = (m1 << 16) | 0xffff;
    } else {
      m2 = strtoul(p+1, &p, 16);
      *maskptr = (m1 << 16) | m2;
    }
  }

  return p;
}

static struct match *
parse_config(const char *filename)
{
  char line[MAX_LINE], *p;
  FILE *f;
  struct match *list = NULL;
  struct match **ep = &list;
  struct match *m;

  if ( !filename )
    filename = get_config();

  f = fopen(filename, "r");
  if ( !f )
    return list;

  while ( fgets(line, sizeof line, f) ) {
    p = skipspace(line);

    if ( !looking_at(p, "#") )
      continue;
    p = skipspace(p+1);

    if ( !looking_at(p, "dev") )
      continue;
    p = skipspace(p+3);

    m = malloc(sizeof(struct match));
    if ( !m )
      continue;

    memset(m, 0, sizeof *m);
    m->rid_max = 0xff;

    for(;;) {
      p = skipspace(p);

      if ( looking_at(p, "did") ) {
	p = get_did(p+3, &m->did, &m->did_mask);
	m->did_mask = 0xffffffff;
      } else if ( looking_at(p, "sid") ) {
	p = get_did(p+3, &m->sid, &m->sid_mask);
      } else if ( looking_at(p, "rid") ) {
	unsigned long r0, r1;

	p = skipspace(p+3);

	r0 = strtoul(p, &p, 16);
	if ( *p == '-' ) {
	  r1 = strtoul(p+1, &p, 16);
	} else {
	  r1 = r0;
	}

	m->rid_min = r0;
	m->rid_max = r1;
      } else {
	char *e;

	e = strchr(p, '\n');
	if ( *e ) *e = '\0';
	e = strchr(p, '\r');
	if ( *e ) *e = '\0';

	m->filename = strdup(p);
	if ( !m->filename )
	  m->did = -1;
	break;			/* Done with this line */
      }
    }

    dprintf("DEV DID %08x/%08x SID %08x/%08x RID %02x-%02x CMD %s\n",
	    m->did, m->did_mask, m->sid, m->sid_mask,
	    m->rid_min, m->rid_max, m->filename);

    *ep = m;
    ep = &m->next;
  }

  return list;
}

static struct match *
pciscan(struct match *list)
{
  unsigned int bus, dev, func, maxfunc;
  uint32_t did, sid;
  uint8_t hdrtype, rid;
  pciaddr_t a;
  struct match *m;
  int cfgtype;

#ifdef DEBUG
  outl(~0, 0xcf8);
  printf("Poking at port CF8 = %#08x\n", inl(0xcf8));
  outl(0, 0xcf8);
#endif

  cfgtype = pci_set_config_type(PCI_CFG_AUTO);
  (void)cfgtype;

  dprintf("PCI configuration type %d\n", cfgtype);

  for ( bus = 0 ; bus <= 0xff ; bus++ ) {

    dprintf("Probing bus 0x%02x... \n", bus);

    for ( dev = 0 ; dev <= 0x1f ; dev++ ) {
      maxfunc = 0;
      for ( func = 0 ; func <= maxfunc ; func++ ) {
	a = pci_mkaddr(bus, dev, func, 0);

	did = pci_readl(a);

	if ( did == 0xffffffff || did == 0xffff0000 ||
	     did == 0x0000ffff || did == 0x00000000 )
	  continue;

	hdrtype = pci_readb(a + 0x0e);

	if ( hdrtype & 0x80 )
	  maxfunc = 7;		/* Multifunction device */

	if ( hdrtype & 0x7f )
	  continue;		/* Ignore bridge devices */

	rid = pci_readb(a + 0x08);
	sid = pci_readl(a + 0x2c);

	dprintf("Scanning: DID %08x SID %08x RID %02x\n", did, sid, rid);

	for ( m = list ; m ; m = m->next ) {
	  if ( ((did ^ m->did) & m->did_mask) == 0 &&
	       ((sid ^ m->sid) & m->sid_mask) == 0 &&
	       rid >= m->rid_min && rid <= m->rid_max )
	    return m;
	}
      }
    }
  }

  return NULL;
}

static void __attribute__((noreturn))
execute(const char *cmdline)
{
  static com32sys_t ireg;

  strcpy(__com32.cs_bounce, cmdline);
  ireg.eax.w[0] = 0x0003;       /* Run command */
  ireg.ebx.w[0] = OFFS(__com32.cs_bounce);
  ireg.es = SEG(__com32.cs_bounce);
  __intcall(0x22, &ireg, NULL);
  exit(255);  /* Shouldn't return */
}

int main(int argc, char *argv[])
{
  struct match *list, *match;

  openconsole(&dev_null_r, &dev_stdcon_w);

  list = parse_config(argc < 2 ? NULL : argv[1]);

  match = pciscan(list);

  if ( match )
    execute(match->filename);

  /* On error, return to the command line */
  fputs("Error: no recognized network card found!\n", stderr);
  return 1;
}
