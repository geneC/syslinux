/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#define _GNU_SOURCE		/* Needed for asprintf() on Linux */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minmax.h>
#include <alloca.h>
#include <inttypes.h>
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
int hiddenmenu   = 0;
long long totaltimeout = 0;

char *ontimeout   = NULL;
char *onerror     = NULL;

char *menu_master_passwd = NULL;
char *menu_background = NULL;

struct fkey_help fkeyhelp[12];

struct menu_entry menu_entries[MAX_ENTRIES];
struct menu_entry hide_entries[MAX_ENTRIES];
struct menu_entry *menu_hotkeys[256];

struct messages messages[MSG_COUNT] = {
  [MSG_TITLE] =
  { "title", "", NULL },
  [MSG_AUTOBOOT] =
  { "autoboot", "Automatic boot in # second{,s}...", NULL },
  [MSG_TAB] =
  { "tabmsg", "Press [Tab] to edit options", NULL },
  [MSG_NOTAB] =
  { "notabmsg", "", NULL },
  [MSG_PASSPROMPT] =
  { "passprompt", "Password required", NULL },
};

#define astrdup(x) ({ char *__x = (x); \
                      size_t __n = strlen(__x) + 1; \
                      char *__p = alloca(__n); \
                      if ( __p ) memcpy(__p, __x, __n); \
                      __p; })

/* Must match enum kernel_type */
const char *kernel_types[] = {
  "none",
  "localboot",
  "kernel",
  "linux",
  "boot",
  "bss",
  "pxe",
  "fdimage",
  "comboot",
  "com32",
  "config",
  NULL
};

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
  enum kernel_type type;
  char *append;
  char *menulabel;
  char *passwd;
  char *helptext;
  unsigned int ipappend;
  unsigned int menuhide;
  unsigned int menudefault;
  unsigned int menuseparator;
  unsigned int menudisabled;
  unsigned int menuindent;
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
    me->helptext    = ld->helptext;
    me->hotkey = 0;
    me->disabled = 0;

    if ( ld->menuindent ) {
      char *n = (char *)malloc(ld->menuindent + strlen(me->displayname) + 1);
      memset(n, 32, ld->menuindent);
      strcpy(n + ld->menuindent, me->displayname);
      me->displayname = n;
    }

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
    if (ld->type == KT_KERNEL) {
      asprintf(&me->cmdline, "%s%s%s%s",
	       ld->kernel, s, a, ipoptions);
    } else {
      asprintf(&me->cmdline, ".%s %s%s%s%s",
	       kernel_types[ld->type], ld->kernel, s, a, ipoptions);
    }

    if ( ld->menuseparator )
      me->displayname = "";

    if ( ld->menuseparator || ld->menudisabled ) {
      me->label    = NULL;
      me->passwd   = NULL;
      me->disabled = 1;

      if ( me->cmdline )
        free(me->cmdline);

      me->cmdline = NULL;
    }

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

      if ( ld->menudefault && !ld->menudisabled && !ld->menuseparator )
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

int my_isxdigit(char c)
{
  unsigned int uc = c;

  return (uc-'0') < 10 ||
    ((uc|0x20)-'a') < 6;
}

unsigned int hexval(char c)
{
  unsigned char uc = c | 0x20;
  unsigned int v;

  v = uc-'0';
  if (v < 10)
    return v;

  return uc-'a'+10;
}

unsigned int hexval2(const char *p)
{
  return (hexval(p[0]) << 4)+hexval(p[1]);
}

uint32_t parse_argb(char **p)
{
  char *sp = *p;
  char *ep;
  uint32_t argb;
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
      0xff000000 +
      (hexval(sp[0])*0x11 << 16) +
      (hexval(sp[1])*0x11 << 8) +
      (hexval(sp[2])*0x11);
    break;
  case 4:			/* #argb */
    argb =
      (hexval(sp[0])*0x11 << 24) +
      (hexval(sp[1])*0x11 << 16) +
      (hexval(sp[2])*0x11 << 8) +
      (hexval(sp[3])*0x11);
    break;
  case 6:			/* #rrggbb */
  case 9:			/* #rrrgggbbb */
  case 12:			/* #rrrrggggbbbb */
    dl = len/3;
    argb =
      0xff000000 +
      (hexval2(sp+0) << 16) +
      (hexval2(sp+dl) << 8) +
      hexval2(sp+dl*2);
    break;
  case 8:			/* #aarrggbb */
    /* #aaarrrgggbbb is indistinguishable from #rrrrggggbbbb,
       assume the latter is a more common format */
  case 16:			/* #aaaarrrrggggbbbb */
    dl = len/4;
    argb =
      (hexval2(sp+0) << 24) +
      (hexval2(sp+dl) << 16) +
      (hexval2(sp+dl*2) << 8) +
      hexval2(sp+dl*3);
    break;
  default:
    argb = 0xffff0000;		/* Bright red (error indication) */
    break;
  }

  return argb;
}

/*
 * Parser state.  This is global so that including multiple
 * files work as expected, which is that everything works the
 * same way as if the files had been concatenated together.
 */
static char *append = NULL;
static unsigned int ipappend = 0;
static struct labeldata ld;

static int parse_one_config(const char *filename);

static char *is_kernel_type(char *cmdstr, enum kernel_type *type)
{
  const char **p;
  char *q;
  enum kernel_type t = KT_NONE;

  for (p = kernel_types; *p; p++, t++) {
    if ((q = looking_at(cmdstr, *p))) {
      *type = t;
      return q;
    }
  }

  return NULL;
}

static char *is_message_name(char *cmdstr, struct messages **msgptr)
{
  char *q;
  int i;

  for (i = 0; i < MSG_COUNT; i++) {
    if ((q = looking_at(cmdstr, messages[i].name))) {
      *msgptr = &messages[i];
      return q;
    }
  }

  return NULL;
}

static char *is_fkey(char *cmdstr, int *fkeyno)
{
  char *q;
  int no;

  if ((cmdstr[0]|0x20) != 'f')
    return NULL;

  no = strtoul(cmdstr+1, &q, 10);
  if (!my_isspace(*q))
    return NULL;

  if (no < 0 || no > 12)
    return NULL;

  *fkeyno = (no == 0) ? 10 : no-1;
  return q;
}

static void parse_config_file(FILE *f)
{
  char line[MAX_LINE], *p, *ep, ch;
  enum kernel_type type;
  struct messages *msgptr;
  int fkeyno;

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

      if ( looking_at(p, "label") ) {
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
      } else if ( (ep = looking_at(p, "include")) ) {
	p = skipspace(ep);
	parse_one_config(p);
      } else if ( (ep = looking_at(p, "background")) ) {
	p = skipspace(ep);
	if (menu_background)
	  free(menu_background);
	menu_background = dup_word(&p);
      } else if ( (ep = looking_at(p, "hidden")) ) {
	hiddenmenu = 1;
      } else if ( (ep = is_message_name(p, &msgptr)) ) {
	free(msgptr->msg);
	msgptr->msg = strdup(skipspace(ep));
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
	      if (looking_at(p, "*")) {
		p++;
	      } else {
		free((void *)cptr->ansi);
		cptr->ansi = dup_word(&p);
	      }

	      p = skipspace(p);
	      if (*p) {
		if (looking_at(p, "*"))
		  p++;
		else
		  cptr->argb_fg = parse_argb(&p);

		p = skipspace(p);
		if (*p) {
		  if (looking_at(p, "*"))
		    p++;
		  else
		    cptr->argb_bg = parse_argb(&p);

		  /* Parse a shadow mode */
		  p = skipspace(p);
		  ch = *p | 0x20;
		  if (ch == 'n')	/* none */
		    cptr->shadow = SHADOW_NONE;
		  else if (ch == 's') /* std, standard */
		    cptr->shadow = SHADOW_NORMAL;
		  else if (ch == 'a') /* all */
		    cptr->shadow = SHADOW_ALL;
		  else if (ch == 'r') /* rev, reverse */
		    cptr->shadow = SHADOW_REVERSE;
		}
	      }
	    }
	    break;
	  }
	  cptr++;
	}
      } else if ((ep = looking_at(p, "msgcolor")) ||
		 (ep = looking_at(p, "msgcolour"))) {
	unsigned int fg_mask = MSG_COLORS_DEF_FG;
	unsigned int bg_mask = MSG_COLORS_DEF_BG;
	enum color_table_shadow shadow = MSG_COLORS_DEF_SHADOW;

	p = skipspace(ep);
	if (*p) {
	  if (!looking_at(p, "*"))
	    fg_mask = parse_argb(&p);

	  p = skipspace(p);
	  if (*p) {
	    if (!looking_at(p, "*"))
	      bg_mask = parse_argb(&p);

	    p = skipspace(p);
	    switch (*p | 0x20) {
	    case 'n':
	      shadow = SHADOW_NONE;
	      break;
	    case 's':
	      shadow = SHADOW_NORMAL;
	      break;
	    case 'a':
	      shadow = SHADOW_ALL;
	      break;
	    case 'r':
	      shadow = SHADOW_REVERSE;
	      break;
	    default:
	      /* go with default */
	      break;
	    }
	  }
	}
	set_msg_colors_global(fg_mask, bg_mask, shadow);
      } else if ( looking_at(p, "separator") ) {
        record(&ld, append);
        memset(&ld, 0, sizeof(struct labeldata));
        ld.label = "";
	ld.menuseparator = 1;
        record(&ld, append);
        memset(&ld, 0, sizeof(struct labeldata));
      } else if ( looking_at(p, "disable") ||
		  looking_at(p, "disabled")) {
	ld.menudisabled = 1;
      } else if ( looking_at(p, "indent") ) {
        ld.menuindent = atoi(skipspace(p+6));
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
    } else if ( looking_at(p, "text") ) {
      enum text_cmd {
	TEXT_UNKNOWN,
	TEXT_HELP
      } cmd = TEXT_UNKNOWN;
      int len = ld.helptext ? strlen(ld.helptext) : 0;
      int xlen;

      p = skipspace(p+4);

      if (looking_at(p, "help"))
	cmd = TEXT_HELP;

      while ( fgets(line, sizeof line, f) ) {
	p = skipspace(line);
	if (looking_at(p, "endtext"))
	  break;

	xlen = strlen(line);

	switch (cmd) {
	case TEXT_UNKNOWN:
	  break;
	case TEXT_HELP:
	  ld.helptext = realloc(ld.helptext, len+xlen+1);
	  memcpy(ld.helptext+len, line, xlen+1);
	  len += xlen;
	  break;
	}
      }
    } else if ( (ep = is_fkey(p, &fkeyno)) ) {
      p = skipspace(ep);
      if (fkeyhelp[fkeyno].textname) {
	free((void *)fkeyhelp[fkeyno].textname);
	fkeyhelp[fkeyno].textname = NULL;
      }
      if (fkeyhelp[fkeyno].background) {
	free((void *)fkeyhelp[fkeyno].background);
	fkeyhelp[fkeyno].background = NULL;
      }

      fkeyhelp[fkeyno].textname = dup_word(&p);
      if (*p) {
	p = skipspace(p);
	fkeyhelp[fkeyno].background = dup_word(&p);
      }
    } else if ( (ep = looking_at(p, "include")) ) {
      p = skipspace(ep);
      parse_one_config(p);
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
      ld.type      = KT_KERNEL;
      ld.passwd    = NULL;
      ld.append    = NULL;
      ld.menulabel = NULL;
      ld.helptext  = NULL;
      ld.ipappend  = ipappend;
      ld.menudefault = ld.menuhide = ld.menuseparator =
	ld.menudisabled = ld.menuindent = 0;
    } else if ( (ep = is_kernel_type(p, &type)) ) {
      if ( ld.label ) {
	free(ld.kernel);
	ld.kernel = strdup(skipspace(ep));
	ld.type = type;
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
    }
  }
}

static int parse_one_config(const char *filename)
{
  FILE *f;

  if (!strcmp(filename, "~"))
    filename = get_config();

  f = fopen(filename, "r");
  if ( !f )
    return -1;

  parse_config_file(f);
  fclose(f);

  return 0;
}

void parse_configs(char **argv)
{
  const char *filename;
  int i;

  /* Initialize defaults */

  for (i = 0; i < MSG_COUNT; i++) {
    if (messages[i].msg)
      free(messages[i].msg);
    messages[i].msg = strdup(messages[i].defmsg);
  }

  /* Other initialization */

  get_ipappend();
  memset(&ld, 0, sizeof(struct labeldata));

  /* Actually process the files */

  if ( !*argv ) {
    parse_one_config("~");
  } else {
    while ( (filename = *argv++) )
      parse_one_config(filename);
  }

  /* On final EOF process the last label statement */

  record(&ld, append);

  /* Common postprocessing */

  if ( ontimeout )
    ontimeout = unlabel(ontimeout);
  if ( onerror )
    onerror = unlabel(onerror);
}
