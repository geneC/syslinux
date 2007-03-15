#ifndef _NETINET_IN_H
#define _NETINET_IN_H

/* COM32 will be running on an i386 platform */

#include <stdint.h>

static inline uint16_t __htons(uint16_t v)
{
  return ((v) << 8) | ((v) >> 8);
}

#define htons(x) __htons(x)
#define ntohs(x) __htons(x)

static inline uint32_t __htonl(uint32_t v)
{
  if ( __builtin_constant_p(v) ) {
    return (((v) & 0x000000ff) << 24) |
      (((v) & 0x0000ff00) << 8) |
      (((v) & 0x00ff0000) >> 8) |
      (((v) & 0xff000000) >> 24);
  } else {
    asm("xchgb %h0,%b0 ; roll $16,%0 ; xchgb %h0,%b0" : "+abcd" (v));
    return v;
  }
}

#define htonl(x) __htonl(x)
#define ntohl(x) __htonl(x)

static inline uint64_t __htonq(uint64_t v)
{
  return ((uint64_t) __htonl(v) << 32) | __htonl(v >> 32);
}

#define htonq(x) __htonq(x)
#define ntohq(x) __htonq(x)

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
  in_addr_t s_addr;
};

#endif /* _NETINET_IN_H */
