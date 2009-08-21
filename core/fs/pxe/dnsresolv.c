#include <stdio.h>
#include <string.h>
#include <core.h>
#include "pxe.h"

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
 * number of dots encountered. On return, both src and dst are updated.
 */
int dns_mangle(char **dst, char **src)
{
    char *p = *src;
    char *q = *dst;
    char *count_ptr;
    char c;
    int dots = 0;    

    count_ptr = q;
    *q++ = 0;

    while (1) {
        c = *p++;
        if (c == 0 || c == ':')
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
    *src = --p;
    *dst = q;
    return dots;
}
    

/*
 * Compare two sets of DNS labels, in _s1_ and _s2_; the one in _s1_
 * is allowed pointers relative to a packet in DNSRecvBuf.
 *
 */
static int dns_compare(char *s1, char *s2)
{
#if 0
    while (1) {
        if (*s1 < 0xc0)
            break;
        s1 = DNSRecvBuf + (((*s1++ & 0x3f) << 8) | (*s1++));
    }
    if (*s1 == 0)
        return 1;
    else if (*s1++ != *s2++)
        return 0; /* not same */
    else
        return !strcmp(s1, s2);
#else
    (void)s1;
    (void)s2;
    return 1;
#endif
}

/*
 * Skip past a DNS label set in DS:SI
 */
static char *dns_skiplabel(char *label)
{
    uint8_t c;
    
    while (1) {
        c = *label++;
        if (c >= 0xc0)
            return ++label; /* pointer is two bytes */
        if (c == 0)
            return label;
        label += c;
    }
}

/*
 * Actual resolver function
 * Points to a null-terminated or :-terminated string in _name_
 * and returns the ip addr in _ip_ if it exists and can be found.
 * If _ip_ = 0 on exit, the lookup failed. _name_ will be updated
 *
 */
uint32_t dns_resolv(char **name)
{
    char *p;
    int err;
    int dots;
    int same;
    int rd_len;
    int ques, reps;    /* number of questions and replies */
    uint8_t timeout;
    const uint8_t *timeout_ptr = TimeoutTable;
    uint16_t oldtime;
    uint32_t srv;
    uint32_t *srv_ptr = dns_server;
    struct dnshdr *hd1 = (struct dnshdr *)DNSSendBuf;
    struct dnshdr *hd2 = (struct dnshdr *)DNSRecvBuf; 
    struct dnsquery *query;
    struct dnsrr *rr;
    static __lowmem struct pxe_udp_write_pkt uw_pkt;
    static __lowmem struct pxe_udp_read_pkt ur_pkt;

    /* First, fill the DNS header struct */
    hd1->id++;                      /* New query ID */
    hd1->flags   = htons(0x0100);   /* Recursion requested */
    hd1->qdcount = htons(1);        /* One question */
    hd1->ancount = 0;               /* No answers */
    hd1->nscount = 0;               /* No NS */
    hd1->arcount = 0;               /* No AR */
    
    p = DNSSendBuf + sizeof(struct dnshdr);
    dots = dns_mangle(&p, name);   /* store the CNAME */
        
    if (!dots) {
        p--; /* Remove final null */
        /* Uncompressed DNS label set so it ends in null */
        strcpy(p, LocalDomain); 
    }
    
    /* Fill the DNS query packet */
    query = (struct dnsquery *)p;
    query->qtype  = htons(1);  /* QTYPE  = 1 = A */
    query->qclass = htons(1);  /* QCLASS = 1 = IN */
    p += sizeof(struct dnsquery);
   
    /* Now send it to name server */
    timeout_ptr = TimeoutTable;
    timeout = *timeout_ptr++;
    while (srv_ptr < dns_server + DNS_MAX_SERVERS) {
        srv = *srv_ptr++;
        uw_pkt.status     = 0;
        uw_pkt.sip        = srv;
        uw_pkt.gip        = ((srv ^ MyIP) & Netmask) ? Gateway : 0;
        uw_pkt.lport      = DNS_LOCAL_PORT;
        uw_pkt.rport      = DNS_PORT;
        uw_pkt.buffersize = p - DNSSendBuf;
        uw_pkt.buffer[0]  = OFFS_WRT(DNSSendBuf, 0);
        uw_pkt.buffer[1]  = 0;
        err = pxe_call(PXENV_UDP_WRITE, &uw_pkt);
        if (err || uw_pkt.status != 0)
            continue;
        
        oldtime = BIOS_timer;
        while (oldtime + timeout >= BIOS_timer) {
            ur_pkt.status     = 0;
            ur_pkt.sip        = srv;
            ur_pkt.dip        = MyIP;
            ur_pkt.rport      = DNS_PORT;
            ur_pkt.lport      = DNS_LOCAL_PORT;
            ur_pkt.buffersize = DNS_MAX_PACKET;
            ur_pkt.buffer[0]  = OFFS_WRT(DNSRecvBuf, 0);
            ur_pkt.buffer[1]  = 0;
            err = pxe_call(PXENV_UDP_READ, &ur_pkt);
            if (err || ur_pkt.status)
                continue;
            
            /* Got a packet, deal with it... */
            if (hd2->id == hd1->id)
                break;
        }
        if (BIOS_timer > oldtime + timeout) {
            /* time out */        
            timeout = *timeout_ptr++;
            if (!timeout)
                return 0;     /* All time ticks run out */
            else 
                continue;     /* try next */
        }
        if ((hd2->flags ^ 0x80) & htons(0xf80f))
            goto badness;        

        ques = htons(hd2->qdcount);   /* Questions */
        reps = htons(hd2->ancount);   /* Replies   */
        p = DNSRecvBuf + sizeof(struct dnshdr);
        while (ques--) {
            p = dns_skiplabel(p); /* Skip name */
            p += 4;               /* Skip question trailer */
        }

        /* Parse the replies */
        while (reps--) {
            same = dns_compare(p, (char *)(DNSSendBuf + sizeof(struct dnshdr)));
            p = dns_skiplabel(p);
            rr = (struct dnsrr *)p;
            rd_len = htons(rr->rdlength);
            if (same && rd_len == 4   &&
                htons(rr->type) == 1  && /* TYPE  == A */
                htons(rr->class) == 1 )  /* CLASS == IN */
                return *(uint32_t *)rr->rdata;
            
            /* not the one we want, try next */
            p += sizeof(struct dnsrr) + rd_len;
        }

    badness:
        /*
         *
         ; We got back no data from this server.
         ; Unfortunately, for a recursive, non-authoritative
         ; query there is no such thing as an NXDOMAIN reply,
         ; which technically means we can't draw any
         ; conclusions.  However, in practice that means the
         ; domain doesn't exist.  If this turns out to be a
         ; problem, we may want to add code to go through all
         ; the servers before giving up.
         
         ; If the DNS server wasn't capable of recursion, and
         ; isn't capable of giving us an authoritative reply
         ; (i.e. neither AA or RA set), then at least try a
         ; different setver...
        */
        if (hd2->flags == htons(0x480)) 
            continue;

        break; /* failed */
    }

    return 0;
}
    
    
/*
 * the one should be called from ASM file 
 */
void pxe_dns_resolv(com32sys_t *regs)
{
    char *name = MK_PTR(regs->ds, regs->esi.w[0]);
    
    regs->eax.l = dns_resolv(&name);
}
