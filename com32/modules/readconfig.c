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
#include <alloca.h>
#ifdef __COM32__
# include <com32.h>
#endif

#include "menu.h"

int nentries  = 0;
int defentry  = 0;
int allowedit = 1;		/* Allow edits of the command line */
int timeout   = 0;

char *menu_title = "";
char *ontimeout  = NULL;

struct menu_entry menu_entries[MAX_ENTRIES];

#define astrdup(x) ({ char *__x = (x); \
                      size_t __n = strlen(__x) + 1; \
                      char *__p = alloca(__n); \
                      if ( __p ) memcpy(__p, __x, __n); \
                      __p; })
                      

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
static int looking_at(const char *line, const char *kwd)
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

static void record(char *label, char *lkernel, char *lappend, char *append)
{
  if ( label ) {
    char *a, *s;
    menu_entries[nentries].displayname = label;
    a = lappend;
    if ( !a ) a = append;
    if ( !a || (a[0] == '-' && !a[1]) ) a = "";
    s = a[0] ? " " : "";
    asprintf(&menu_entries[nentries].cmdline, "%s%s%s", lkernel, s, a);

    printf("displayname: %s\n", menu_entries[nentries].displayname);
    printf("cmdline:     %s\n", menu_entries[nentries].cmdline);

    label = NULL;
    free(lkernel);
    if ( lappend )
      free(lappend);
    nentries++;
  }
}

void parse_config(const char *filename)
{
  char line[MAX_LINE], *p;
  FILE *f;
  char *append = NULL;
  char *label = NULL, *lkernel = NULL, *lappend = NULL;

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
    printf("> %s\n", p);

    if ( looking_at(p, "menu") ) {
      p = skipspace(line+4);

      if ( looking_at(p, "title") ) {
	menu_title = strdup(skipspace(p+5));
      } else if ( looking_at(p, "default") ) {
	defentry = atoi(skipspace(p+7));
      } else {
	/* Unknown, ignore for now */
      }
    } else if ( looking_at(p, "append") ) {
      char *a = strdup(skipspace(p+6));
      if ( label )
	lappend = a;
      else
	append = a;
    } else if ( looking_at(p, "label") ) {
      p = skipspace(p+5);
      record(label, lkernel, lappend, append);
      label   = strdup(p);
      lkernel = strdup(p);
      lappend = NULL;
    } else if ( looking_at(p, "kernel") ) {
      if ( label ) {
	free(lkernel);
	lkernel = strdup(skipspace(p+6));
      }
    } else if ( looking_at(p, "timeout") ) {
      timeout = atoi(skipspace(p+7));
    } else if ( looking_at(p, "ontimeout") ) {
      ontimeout = strdup(skipspace(p+9));
    } else if ( looking_at(p, "allowoptions") ) {
      allowedit = atoi(skipspace(p+12));
    }
  }
  
  record(label, lkernel, lappend, append);
  fclose(f);
}
