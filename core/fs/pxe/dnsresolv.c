#include <stdio.h>
#include <string.h>
#include <core.h>
#include "pxe.h"
#include "lwip/api.h"
#include "lwip/dns.h"

/* DNS CLASS values we care about */
#define CLASS_IN	1

/* DNS TYPE values we care about */
#define TYPE_A		1
#define TYPE_CNAME	5

/*
 * The DNS header structure
 */
struct dnshdr {
    uint16_t id;
    uint16_t flags;
    /* number of entries in the question section */
    uint16_t qdcount;
    /* number of resource records in the answer section */
    uint16_t ancount;
    /* number of name server resource records in the authority records section*/
    uint16_t nscount;
    /* number of resource records in the additional records section */
    uint16_t arcount;
} __attribute__ ((packed));

/*
 * The DNS query structure
 */
struct dnsquery {
    uint16_t qtype;
    uint16_t qclass;
} __attribute__ ((packed));

/*
 * The DNS Resource recodes structure
 */
struct dnsrr {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;   /* The lenght of this rr data */
    char     rdata[];
} __attribute__ ((packed));


uint32_t dns_server[DNS_MAX_SERVERS] = {0, };


/*
 * Turn a string in _src_ into a DNS "label set" in _dst_; returns the
 * number of dots encountered. On return, *dst is updated.
 */
int dns_mangle(char **dst, const char *p)
{
    char *q = *dst;
    char *count_ptr;
    char c;
    int dots = 0;

    count_ptr = q;
    *q++ = 0;

    while (1) {
        c = *p++;
        if (c == 0 || c == ':' || c == '/')
            break;
        if (c == '.') {
            dots++;
            count_ptr = q;
            *q++ = 0;
            continue;
        }

        *count_ptr += 1;
        *q++ = c;
    }

    if (*count_ptr)
        *q++ = 0;

    /* update the strings */
    *dst = q;
    return dots;
}

/*
 * Actual resolver function
 * Points to a null-terminated or :-terminated string in _name_
 * and returns the ip addr in _ip_ if it exists and can be found.
 * If _ip_ = 0 on exit, the lookup failed. _name_ will be updated
 */
uint32_t dns_resolv(const char *name)
{
    err_t err;
    struct ip_addr ip;
    char dns_name[PKTBUF_SIZE];
    const char *src;
    char *dst;

    /* Make sure we have at least one valid DNS server */
    if (!dns_getserver(0).addr)
	return 0;

    /* Copy the name to look up to ensure it is null terminated */
    for (dst = dns_name, src = name; *src; src++, dst++) {
	int ch = *src;
	if (ch == '\0' || ch == ':' || ch == '/') {
	    *dst = '\0';
	    break;
	}
	*dst = ch;
    }

    err = netconn_gethostbyname(dns_name, &ip);
    if (err)
	return 0;

    return ip.addr;
}


/*
 * the one should be called from ASM file
 */
void pxe_dns_resolv(com32sys_t *regs)
{
    const char *name = MK_PTR(regs->ds, regs->esi.w[0]);

    regs->eax.l = dns_resolv(name);
}
