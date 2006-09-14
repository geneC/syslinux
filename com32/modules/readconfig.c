/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2006 H. Peter Anvin - All Rights Reserved
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
#include <colortbl.h>
#ifdef __COM32__
# include <com32.h>
#endif

#include "menu.h"

int nentries     = 0;
int nhidden      = 0;
int defentry     = 0;
int allowedit    = 1;		/* Allow edits of the command line */
int timeout      = 0;
int shiftkey     = 0;		/* Only display menu if shift key pressed */
long long totaltimeout = 0;

char *menu_title  = "";
char *ontimeout   = NULL;
char *onerror     = NULL;

char *menu_master_passwd = NULL;

char *menu_background = NULL;

struct menu_entry menu_entries[MAX_ENTRIES];
struct menu_entry hide_entries[MAX_ENTRIES];
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
  while (*p && my_isspace(*p))
    p++;

  return p;
}

/* Check to see if we are at a certain keyword (case insensitive) */
/* Returns a pointer to the first character past the keyword */
static char *
looking_at(char *line, const char *kwd)
{
  char *p = line;
  const char *q = kwd;

  while ( *p && *q && ((*p^*q) & ~0x20) == 0 ) {
    p++;
    q++;
  }

  if ( *q )
    return NULL;		/* Didn't see the keyword */

  return my_isspace(*p) ? p : NULL; /* Must be EOL or whitespace */
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
      unsigned char *p = (unsigned char *)strchr(ld->menulabel, '^');
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
    asprintf(&me->cmdline, "%s%s%s%s", ld->kernel, s, a, ipoptions);

    ld->label = NULL;
    ld->passwd = NULL;

    free(ld->kernel);
    ld->kernel = NULL;

    if ( ld->append ) {
      free(ld->append);
      ld->append = NULL;
    }

    if ( !ld->menuhide ) {
      if ( me->hotkey )
	menu_hotkeys[me->hotkey] = me;

      if ( ld->menudefault )
	defentry = nentries;

      nentries++;
    }
    else {
      hide_entries[nhidden].displayname = me->displayname;
      hide_entries[nhidden].label       = me->label;
      hide_entries[nhidden].cmdline     = me->cmdline;
      hide_entries[nhidden].passwd      = me->passwd;

      me->displayname = NULL;
      me->label       = NULL;
      me->cmdline     = NULL;
      me->passwd      = NULL;

      nhidden++;
    }
  }
}

static char *
unlabel(char *str)
{
  /* Convert a CLI-style command line to an executable command line */
  const char *p;
  char *q;
  struct menu_entry *me;
  int i, pos;

  p = str;
  while ( *p && !my_isspace(*p) )
    p++;

  /* p now points to the first byte beyond the kernel name */
  pos = p-str;

  for ( i = 0 ; i < nentries ; i++ ) {
    me = &menu_entries[i];

    if ( !strncmp(str, me->label, pos) && !me->label[pos] ) {
      /* Found matching label */
      q = malloc(strlen(me->cmdline) + strlen(p) + 1);
      strcpy(q, me->cmdline);
      strcat(q, p);

      free(str);

      return q;
    }
  }

  for ( i = 0 ; i < nhidden ; i++ ) {
    me = &hide_entries[i];

    if ( !strncmp(str, me->label, pos) && !me->label[pos] ) {
      /* Found matching label */
      q = malloc(strlen(me->cmdline) + strlen(p) + 1);
      strcpy(q, me->cmdline);
      strcat(q, p);

      free(str);

      return q;
    }
  }

  return str;
}

static char *
dup_word(char **p)
{
  char *sp = *p;
  char *ep = sp;
  char *dp;
  size_t len;

  while (*ep && !my_isspace(*ep))
    ep++;

  *p = ep;
  len = ep-sp;

  dp = malloc(len+1);
  memcpy(dp, sp, len);
  dp[len] = '\0';

  return dp;
}

static int my_isxdigit(char c)
{
  unsigned char uc = c | 0x20;

  return (uc-'0') < 10 || (uc-'a') < 6;
}

static unsigned int hexval(char c)
{
  unsigned char uc = c | 0x20;

  if (uc & 0x40)
    return uc-'a'+10;
  else
    return uc-'0';
}

static unsigned int hexval2(char *p)
{
  return (hexval(p[0]) << 4)+hexval(p[1]);
}

static unsigned int parse_argb(char **p)
{
  char *sp = *p;
  char *ep;
  unsigned int argb;
  size_t len, dl;

  if (*sp == '#')
    sp++;

  ep = sp;

  while (my_isxdigit(*ep))
    ep++;

  *p = ep;
  len = ep-sp;

  switch(len) {
  case 3:			/* #rgb */
    argb =
      0xff000000 |
      (hexval(sp[0])*0x11 << 16)|
      (hexval(sp[1])*0x11 << 8) |
      (hexval(sp[2])*0x11 << 0);
    break;
  case 4:			/* #argb */
    argb =
      (hexval(sp[0])*0x11 << 24)|
      (hexval(sp[1])*0x11 << 16)|
      (hexval(sp[2])*0x11 << 8) |
      (hexval(sp[3])*0x11 << 0);
    break;
  case 6:			/* #rrggbb */
  case 9:			/* #rrrgggbbb */
  case 12:			/* #rrrrggggbbbb */
    dl = len/3;
    argb =
      0xff000000 |
      (hexval2(sp+0) << 16) |
      (hexval2(sp+dl) << 8)|
      (hexval2(sp+dl*2) << 0);
    break;
  case 8:			/* #aarrggbb */
    /* 12 is indistinguishable from #rrrrggggbbbb,
       assume that is a more common format */
  case 16:			/* #aaaarrrrggggbbbb */
    dl = len/4;
    argb =
      (hexval2(sp+0) << 24) |
      (hexval2(sp+dl) << 16) |
      (hexval2(sp+dl*2) << 8)|
      (hexval2(sp+dl*3) << 0);
    break;
  default:
    argb = 0;
    break;
  }

  return argb;
}

void parse_config(const char *filename)
{
  char line[MAX_LINE], *p, *ep;
  FILE *f;
  char *append = NULL;
  unsigned int ipappend = 0;
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
      } else if ( looking_at(p, "shiftkey") ) {
	shiftkey = 1;
      } else if ( looking_at(p, "onerror") ) {
	onerror = strdup(skipspace(p+7));
      } else if ( looking_at(p, "master") ) {
	p = skipspace(p+6);
	if ( looking_at(p, "passwd") ) {
	  menu_master_passwd = strdup(skipspace(p+6));
	}
      } else if ( (ep = looking_at(p, "background")) ) {
	p = skipspace(ep);
	menu_background = dup_word(&p);
      } else if ((ep = looking_at(p, "color")) ||
		 (ep = looking_at(p, "colour"))) {
	int i;
	struct color_table *cptr;
	p = skipspace(ep);
	cptr = console_color_table;
	for ( i = 0; i < console_color_table_size; i++ ) {
	  if ( (ep = looking_at(p, cptr->name)) ) {
	    p = skipspace(ep);
	    if (*p) {
	      if (!strcmp(p, "*")) {
		p++;
	      } else {
		free((void *)cptr->ansi);
		cptr->ansi = dup_word(&p);
	      }

	      p = skipspace(p);
	      if (*p) {
		if (!strcmp(p, "*")) {
		  p++;
		} else {
		  cptr->argb_fg = parse_argb(&p);
		}

		p = skipspace(p);
		if (*p) {
		  if (strcmp(p, "*"))
		    cptr->argb_bg = parse_argb(&p);
		}
	      }
	    }
	    break;
	  }
	}
      } else {
	/* Unknown, check for layout parameters */
	struct menu_parameter *pp;
	for ( pp = mparm ; pp->name ; pp++ ) {
	  if ( (ep = looking_at(p, pp->name)) ) {
	    pp->value = atoi(skipspace(ep));
	    break;
	  }
	}
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
      ld.passwd    = NULL;
      ld.append    = NULL;
      ld.menulabel = NULL;
      ld.ipappend  = ipappend;
      ld.menudefault = ld.menuhide = 0;
    } else if ( looking_at(p, "kernel") ) {
      if ( ld.label ) {
	free(ld.kernel);
	ld.kernel = strdup(skipspace(p+6));
      }
    } else if ( looking_at(p, "timeout") ) {
      timeout = (atoi(skipspace(p+7))*CLK_TCK+9)/10;
    } else if ( looking_at(p, "totaltimeout") ) {
      totaltimeout = (atoll(skipspace(p+13))*CLK_TCK+9)/10;
    } else if ( looking_at(p, "ontimeout") ) {
      ontimeout = strdup(skipspace(p+9));
    } else if ( looking_at(p, "allowoptions") ) {
      allowedit = atoi(skipspace(p+12));
    } else if ( looking_at(p, "ipappend") ) {
      if (ld.label) 
        ld.ipappend = atoi(skipspace(p+8));
      else
	ipappend = atoi(skipspace(p+8));
    } else if ( looking_at(p, "localboot") ) {
      ld.kernel = strdup(".localboot");
      ld.append = strdup(skipspace(p+9));
    }
  }

  record(&ld, append);
  fclose(f);

  if ( ontimeout )
    ontimeout = unlabel(ontimeout);
  if ( onerror )
    onerror = unlabel(onerror);
}
