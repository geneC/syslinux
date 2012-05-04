/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Liu Aleaxander <Aleaxander@gmail.com>
 *   Copyright 2012 Paulo Alcantara <pcacjr@zytor.com>
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

#include <stdio.h>
#include <string.h>
#include <console.h>
#include <com32.h>

#include <netinet/in.h>
#include <syslinux/pxe.h>

static inline uint32_t dns_resolve(const char *hostname)
{
    return pxe_dns_resolv(hostname);
}

static inline void usage(const char *s)
{
    fprintf(stderr, "Usage: %s hostname [, hostname_1, hostname_2, ...]\n", s);
}

int main(int argc, char *argv[])
{
    int i;
    uint32_t ip;

    openconsole(&dev_null_r, &dev_stdcon_w);

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }

    for (i = 1; i < argc; i++) {
        ip = dns_resolve(argv[i]);
        if (!ip) {
            printf("%s not found.\n", argv[i]);
        } else {
            printf("%-39s %08X %u.%u.%u.%u\n", argv[i], ntohl(ip), ip & 0xFF,
                   (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
        }
    }

    return 0;
}
