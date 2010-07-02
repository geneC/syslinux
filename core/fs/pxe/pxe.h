/* -----------------------------------------------------------------------
 *
 *   Copyright 1999-2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * pxe.h
 *
 * PXE opcodes
 *
 */
#ifndef PXE_H
#define PXE_H

#include <syslinux/pxe_api.h>
#include "fs.h"			/* For MAX_OPEN, should go away */

/*
 * Some basic defines...
 */
#define TFTP_PORT        htons(69)              /* Default TFTP port */
#define TFTP_BLOCKSIZE_LG2 9
#define TFTP_BLOCKSIZE  (1 << TFTP_BLOCKSIZE_LG2)
#define PKTBUF_SIZE     2048			/*  */

#define is_digit(c)     (((c) >= '0') && ((c) <= '9'))

static inline bool is_hex(char c)
{
    return (c >= '0' && c <= '9') ||
	(c >= 'A' && c <= 'F') ||
	(c >= 'a' && c <= 'f');
}

static inline int hexval(char c)
{
    return (c >= 'A') ? (c & ~0x20) - 'A' + 10 : (c - '0');
}

/*
 * TFTP operation codes
 */
#define TFTP_RRQ	 htons(1)		// Read rest
#define TFTP_WRQ	 htons(2)		// Write rest
#define TFTP_DATA	 htons(3)		// Data packet
#define TFTP_ACK	 htons(4)		// ACK packet
#define TFTP_ERROR	 htons(5)		// ERROR packet
#define TFTP_OACK	 htons(6)		// OACK packet

/*
 * TFTP error codes
 */
#define TFTP_EUNDEF	 htons(0)		// Unspecified error
#define TFTP_ENOTFOUND	 htons(1)		// File not found
#define TFTP_EACCESS	 htons(2)		// Access violation
#define TFTP_ENOSPACE	 htons(3)		// Disk full
#define TFTP_EBADOP	 htons(4)		// Invalid TFTP operation
#define TFTP_EBADID	 htons(5)		// Unknown transfer
#define TFTP_EEXISTS	 htons(6)		// File exists
#define TFTP_ENOUSER	 htons(7)		// No such user
#define TFTP_EOPTNEG	 htons(8)		// Option negotiation failure


#define BOOTP_OPTION_MAGIC  htonl(0x63825363)
#define MAC_MAX 32

/* Defines for DNS */
#define DNS_PORT	htons(53)		/* Default DNS port */
#define DNS_MAX_PACKET	512			/* Defined by protocol */
#define DNS_MAX_SERVERS 4			/* Max no of DNS servers */


/*
 * structures 
 */

struct pxenv_t {
    uint8_t    signature[6];	/* PXENV+ */
    uint16_t   version;
    uint8_t    length;
    uint8_t    checksum;
    segoff16_t rmentry;
    uint32_t   pmoffset;
    uint16_t   pmselector;
    uint16_t   stackseg;
    uint16_t   stacksize;
    uint16_t   bc_codeseg;
    uint16_t   bc_codesize;
    uint16_t   bc_dataseg;
    uint16_t   bc_datasize;
    uint16_t   undidataseg;
    uint16_t   undidatasize;
    uint16_t   undicodeseg;
    uint16_t   undicodesize;
    segoff16_t pxeptr;
} __packed;

struct pxe_t {
    uint8_t    signature[4];	/* !PXE */
    uint8_t    structlength;
    uint8_t    structcksum;
    uint8_t    structrev;
    uint8_t    _pad1;
    segoff16_t undiromid;
    segoff16_t baseromid;
    segoff16_t entrypointsp;
    segoff16_t entrypointesp;
    segoff16_t statuscallout;
    uint8_t    _pad2;
    uint8_t    segdesccnt;
    uint16_t   firstselector;
    pxe_segdesc_t  seg[7];
} __packed;

enum pxe_segments {
    PXE_Seg_Stack         = 0,
    PXE_Seg_UNDIData      = 1,
    PXE_Seg_UNDICode      = 2,
    PXE_Seg_UNDICodeWrite = 3,
    PXE_Seg_BC_Data       = 4,
    PXE_Seg_BC_Code       = 5,
    PXE_Seg_BC_CodeWrite  = 6
};

struct bootp_t {
    uint8_t  opcode;        /* BOOTP/DHCP "opcode" */
    uint8_t  hardware;      /* ARP hreadware type */
    uint8_t  hardlen;       /* Hardware address length */
    uint8_t  gatehops;      /* Used by forwarders */
    uint32_t ident;         /* Transaction ID */
    uint16_t seconds;       /* Seconds elapsed */
    uint16_t flags;         /* Broadcast flags */
    uint32_t cip;           /* Cient IP */
    uint32_t yip;           /* "Your" IP */
    uint32_t sip;           /* Next Server IP */
    uint32_t gip;           /* Relay agent IP */
    uint8_t  macaddr[16];   /* Client MAC address */
    uint8_t  sname[64];     /* Server name (optional) */
    char     bootfile[128]; /* Boot file name */
    uint32_t option_magic;  /* Vendor option magic cookie */
    uint8_t  options[1260]; /* Vendor options */
} __attribute__ ((packed));

/*
 * Our inode private information -- this includes the packet buffer!
 */
struct pxe_pvt_inode {
    uint16_t tftp_localport;   /* Local port number  (0=not in us)*/
    uint16_t tftp_remoteport;  /* Remote port number */
    uint32_t tftp_remoteip;    /* Remote IP address */
    uint32_t tftp_filepos;     /* bytes downloaded (includeing buffer) */
    uint32_t tftp_blksize;     /* Block size for this connection(*) */
    uint16_t tftp_bytesleft;   /* Unclaimed data bytes */
    uint16_t tftp_lastpkt;     /* Sequence number of last packet (NBO) */
    char    *tftp_dataptr;     /* Pointer to available data */
    uint8_t  tftp_goteof;      /* 1 if the EOF packet received */
    uint8_t  tftp_unused[3];   /* Currently unused */
    char     tftp_pktbuf[PKTBUF_SIZE];
} __attribute__ ((packed));

#define PVT(i) ((struct pxe_pvt_inode *)((i)->pvt))

/*
 * Network boot information
 */
struct ip_info {
    uint32_t ipv4;
    uint32_t myip;
    uint32_t serverip;
    uint32_t gateway;
    uint32_t netmask;
};

/*
 * Variable externs
 */
extern struct ip_info IPInfo;

extern uint8_t MAC[];
extern char BOOTIFStr[];
extern uint8_t MAC_len;
extern uint8_t MAC_type;

extern uint8_t  DHCPMagic;
extern uint32_t RebootTime;

extern char boot_file[];
extern char path_prefix[];
extern char LocalDomain[];

extern char IPOption[];
extern char dot_quad_buf[];

extern uint32_t dns_server[];

extern uint16_t APIVer;
extern far_ptr_t PXEEntry;
extern uint8_t KeepPXE;

extern far_ptr_t InitStack;

extern bool have_uuid;
extern uint8_t uuid_type;
extern uint8_t uuid[];

extern uint16_t BIOS_fbm;
extern const uint8_t TimeoutTable[];

/*
 * Compute the suitable gateway for a specific route -- too many
 * vendor PXE stacks don't do this correctly...
 */
static inline uint32_t gateway(uint32_t ip)
{
    if ((ip ^ IPInfo.myip) & IPInfo.netmask)
	return IPInfo.gateway;
    else
	return 0;
}

/*
 * functions 
 */

/* pxe.c */
bool ip_ok(uint32_t);
int pxe_call(int, void *);

/* dhcp_options.c */
void parse_dhcp(int);

/* dnsresolv.c */
int dns_mangle(char **, const char *);
uint32_t dns_resolv(const char *);

/* idle.c */
void pxe_idle_init(void);
void pxe_idle_cleanup(void);

/* socknum.c */
uint16_t get_port(void);
void free_port(uint16_t port);

#endif /* pxe.h */
