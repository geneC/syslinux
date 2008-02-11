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

#include <string.h>
#include <sha1.h>
#include <md5.h>
#include <base64.h>

#include "menu.h"

static int passwd_compare_sha1(const char *passwd, const char *entry)
{
  const char *p;
  SHA1_CTX ctx;
  unsigned char sha1[20], pwdsha1[20];

  SHA1Init(&ctx);

  if ( (p = strchr(passwd+3, '$')) ) {
    SHA1Update(&ctx, (void *)passwd+3, p-(passwd+3));
    p++;
  } else {
    p = passwd+3;		/* Assume no salt */
  }

  SHA1Update(&ctx, (void *)entry, strlen(entry));
  SHA1Final(sha1, &ctx);

  memset(pwdsha1, 0, 20);
  unbase64(pwdsha1, 20, p);

  return !memcmp(sha1, pwdsha1, 20);
}

static int passwd_compare_md5(const char *passwd, const char *entry)
{
  const char *crypted = crypt_md5(entry, passwd+3);
  int len = strlen(crypted);

  return !strncmp(crypted, passwd, len) &&
    (passwd[len] == '\0' || passwd[len] == '$');
}

int passwd_compare(const char *passwd, const char *entry)
{
  if ( passwd[0] != '$' )	/* Plaintext passwd, yuck! */
    return !strcmp(entry, passwd);
  else if ( !strncmp(passwd, "$4$", 3) )
    return passwd_compare_sha1(passwd, entry);
  else if ( !strncmp(passwd, "$1$", 3) )
    return passwd_compare_md5(passwd, entry);
  else
    return 0;			/* Invalid encryption algorithm */
}
