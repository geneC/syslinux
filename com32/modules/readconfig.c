#ident "$Id$"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#define _GNU_SOURCE		/* Needed for asprintf() on Linux */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minmax.h>
#include <alloca.h>
#ifdef __COM32__
# include <com32.h>
#endif

#include "menu.h"

int nentries  = 0;
int defentry  = 0;
int allowedit = 1;		/* Allow edits of the command line */
int timeout   = 0;

char *menu_title  = "";
char *ontimeout   = NULL;

char *menu_master_passwd = NULL;

struct menu_entry menu_entries[MAX_ENTRIES];
struct menu_entry *menu_hotkeys[256];

#define astrdup(x) ({ char *__x = (x); \
                      size_t __n = strlen(__x) + 1; \
                      char *__p = alloca(__n); \
                      if ( __p ) memcpy(__p, __x, __n); \
                      __p; })

const char *ipappends[32];
                      
static void
get_ipappend(void)
{
#ifdef __COM32__
  static com32sys_t r;
  uint16_t *ipp;
  int i;
  int nipappends;

  r.eax.w[0] = 0x000F;
  __intcall(0x22, &r, &r);

  nipappends = min(r.ecx.w[0], 32);
  ipp        = MK_PTR(r.es, r.ebx.w[0]);
  for ( i = 0 ; i < nipappends ; i++ ) {
    ipappends[i] = MK_PTR(r.es, *ipp++);
  }
#else
  ipappends[0] = "ip=foo:bar:baz:quux";
  ipappends[1] = "BOOTIF=01-aa-bb-cc-dd-ee-ff";
#endif
}

static const char *
get_config(void)
{
#ifdef __COM32__
  static com32sys_t r;
  
  r.eax.w[0] = 0x000E;
  __intcall(0x22, &r, &r);

  return MK_PTR(r.es, r.ebx.w[0]);
#else
  return "syslinux.cfg";	/* Dummy default name */
#endif
}
  
#define MAX_LINE 512

static char *
skipspace(char *p)
{
  while ( *p && *p <= ' ' )
    p++;
  
  return p;
}

/* Check to see if we are at a certain keyword (case insensitive) */
static int
looking_at(const char *line, const char *kwd)
{
  const char *p = line;
  const char *q = kwd;

  while ( *p && *q && ((*p^*q) & ~0x20) == 0 ) {
    p++;
    q++;
  }

  if ( *q )
    return 0;			/* Didn't see the keyword */

  return *p <= ' ';		/* Must be EOL or whitespace */
}

struct labeldata {
  char *label;
  char *kernel;
  char *append;
  char *menulabel;
  char *passwd;
  unsigned int ipappend;
  unsigned int menuhide;
  unsigned int menudefault;
};

static void
record(struct labeldata *ld, char *append)
{
  char ipoptions[256], *ipp;
  int i;
  struct menu_entry *me = &menu_entries[nentries];

  if ( ld->label ) {
    char *a, *s;
    me->displayname = ld->menulabel ? ld->menulabel : ld->label;
    me->label       = ld->label;
    me->passwd      = ld->passwd;
    me->hotkey = 0;

    if ( ld->menulabel ) {
      unsigned char *p = strchr(ld->menulabel, '^');
      if ( p && p[1] ) {
	int hotkey = p[1] & ~0x20;
	if ( !menu_hotkeys[hotkey] ) {
	  me->hotkey = hotkey;
	}
      }
    }

    ipp = ipoptions;
    *ipp = '\0';
    for ( i = 0 ; i < 32 ; i++ ) {
      if ( (ld->ipappend & (1U << i)) && ipappends[i] )
	ipp += sprintf(ipp, " %s", ipappends[i]);
    }

    a = ld->append;
    if ( !a ) a = append;
    if ( !a || (a[0] == '-' && !a[1]) ) a = "";
    s = a[0] ? " " : "";
    asprintf(&me->cmdline, "%s%s%s%s", ld->kernel, ipoptions, s, a);

    ld->label = NULL;
    free(ld->kernel);
    if ( ld->append )
      free(ld->append);

    if ( !ld->menuhide ) {
      if ( me->hotkey )
	menu_hotkeys[me->hotkey] = me;

      if ( ld->menudefault )
	defentry = nentries;

      nentries++;
    }
  }
}

void parse_config(const char *filename)
{
  char line[MAX_LINE], *p;
  FILE *f;
  char *append = NULL;
  static struct labeldata ld;

  get_ipappend();

  if ( !filename )
    filename = get_config();

  f = fopen(filename, "r");
  if ( !f )
    return;

  while ( fgets(line, sizeof line, f) ) {
    p = strchr(line, '\r');
    if ( p )
      *p = '\0';
    p = strchr(line, '\n');
    if ( p )
      *p = '\0';

    p = skipspace(line);

    if ( looking_at(p, "menu") ) {
      p = skipspace(p+4);
      
      if ( looking_at(p, "title") ) {
	menu_title = strdup(skipspace(p+5));
      } else if ( looking_at(p, "label") ) {
	if ( ld.label )
	  ld.menulabel = strdup(skipspace(p+5));
      } else if ( looking_at(p, "default") ) {
	ld.menudefault = 1;
      } else if ( looking_at(p, "hide") ) {
	ld.menuhide = 1;
      } else if ( looking_at(p, "passwd") ) {
	ld.passwd = strdup(skipspace(p+6));
      } else {
	/* Unknown, ignore for now */
      }
    } else if ( looking_at(p, "append") ) {
      char *a = strdup(skipspace(p+6));
      if ( ld.label )
	ld.append = a;
      else
	append = a;
    } else if ( looking_at(p, "label") ) {
      p = skipspace(p+5);
      record(&ld, append);
      ld.label     = strdup(p);
      ld.kernel    = strdup(p);
      ld.append    = NULL;
      ld.menulabel = NULL;
      ld.ipappend  = ld.menudefault = ld.menuhide = 0;
    } else if ( looking_at(p, "kernel") ) {
      if ( ld.label ) {
	free(ld.kernel);
	ld.kernel = strdup(skipspace(p+6));
      }
    } else if ( looking_at(p, "timeout") ) {
      timeout = atoi(skipspace(p+7));
    } else if ( looking_at(p, "ontimeout") ) {
      ontimeout = strdup(skipspace(p+9));
    } else if ( looking_at(p, "allowoptions") ) {
      allowedit = atoi(skipspace(p+12));
    } else if ( looking_at(p, "ipappend") ) {
      ld.ipappend = atoi(skipspace(p+8));
    }
  }
  
  record(&ld, append);
  fclose(f);
}
