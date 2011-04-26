#ifndef _NETINET_IN_H
#define _NETINET_IN_H

/* COM32 will be running on an i386 platform */

#include <klibc/compiler.h>
#include <klibc/extern.h>
#include <stdint.h>
#include <byteswap.h>

#define htons(x) bswap_16(x)
#define ntohs(x) bswap_16(x)
#define htonl(x) bswap_32(x)
#define ntohl(x) bswap_32(x)
#define htonq(x) bswap_64(x)
#define ntohq(x) bswap_64(x)

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

__extern char *inet_ntoa(struct in_addr);

#endif /* _NETINET_IN_H */
