
/**
* -----------------------------------------------------------------------
*
*   Copyright 1999-2008 H. Peter Anvin - All Rights Reserved
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
*   Boston MA 02111-1307, USA; either version 2 of the License, or
*   (at your option) any later version; incorporated herein by reference.
*
* -----------------------------------------------------------------------

*
* pxe.inc
*
* PXE opcodes
*
*/
#ifndef PXE_H
#define PXE_H


#define PXENV_TFTP_OPEN					 0x0020
#define PXENV_TFTP_CLOSE				 0x0021
#define PXENV_TFTP_READ					 0x0022
#define PXENV_TFTP_READ_FILE				 0x0023
#define PXENV_TFTP_READ_FILE_PMODE			 0x0024
#define PXENV_TFTP_GET_FSIZE				 0x0025

#define PXENV_UDP_OPEN					 0x0030
#define PXENV_UDP_CLOSE					 0x0031
#define PXENV_UDP_READ					 0x0032
#define PXENV_UDP_WRITE					 0x0033

#define PXENV_START_UNDI				 0x0000
#define PXENV_UNDI_STARTUP				 0x0001
#define PXENV_UNDI_CLEANUP				 0x0002
#define PXENV_UNDI_INITIALIZE				 0x0003
#define PXENV_UNDI_RESET_NIC				 0x0004
#define PXENV_UNDI_SHUTDOWN				 0x0005
#define PXENV_UNDI_OPEN					 0x0006
#define PXENV_UNDI_CLOSE				 0x0007
#define PXENV_UNDI_TRANSMIT				 0x0008
#define PXENV_UNDI_SET_MCAST_ADDR			 0x0009
#define PXENV_UNDI_SET_STATION_ADDR			 0x000A
#define PXENV_UNDI_SET_PACKET_FILTER			 0x000B
#define PXENV_UNDI_GET_INFORMATION			 0x000C
#define PXENV_UNDI_GET_STATISTICS			 0x000D
#define PXENV_UNDI_CLEAR_STATISTICS			 0x000E
#define PXENV_UNDI_INITIATE_DIAGS			 0x000F
#define PXENV_UNDI_FORCE_INTERRUPT			 0x0010
#define PXENV_UNDI_GET_MCAST_ADDR			 0x0011
#define PXENV_UNDI_GET_NIC_TYPE				 0x0012
#define PXENV_UNDI_GET_IFACE_INFO			 0x0013
#define PXENV_UNDI_ISR					 0x0014
#define PXENV_STOP_UNDI					 0x0015
#define PXENV_UNDI_GET_STATE				 0x0015

#define PXENV_UNLOAD_STACK				 0x0070
#define PXENV_GET_CACHED_INFO				 0x0071
#define PXENV_RESTART_DHCP				 0x0072
#define PXENV_RESTART_TFTP				 0x0073
#define PXENV_MODE_SWITCH				 0x0074
#define PXENV_START_BASE				 0x0075
#define PXENV_STOP_BASE					 0x0076

/* gPXE extensions... */
#define PXENV_FILE_OPEN					 0x00e0
#define PXENV_FILE_CLOSE				 0x00e1
#define PXENV_FILE_SELECT				 0x00e2
#define PXENV_FILE_READ					 0x00e3
#define PXENV_GET_FILE_SIZE				 0x00e4
#define PXENV_FILE_EXEC					 0x00e5
#define PXENV_FILE_API_CHECK				 0x00e6

/* Exit codes */
#define PXENV_EXIT_SUCCESS				 0x0000
#define PXENV_EXIT_FAILURE				 0x0001

/* Status codes */
#define PXENV_STATUS_SUCCESS				 0x00
#define PXENV_STATUS_FAILURE				 0x01
#define PXENV_STATUS_BAD_FUNC				 0x02
#define PXENV_STATUS_UNSUPPORTED			 0x03
#define PXENV_STATUS_KEEP_UNDI				 0x04
#define PXENV_STATUS_KEEP_ALL				 0x05
#define PXENV_STATUS_OUT_OF_RESOURCES			 0x06
#define PXENV_STATUS_ARP_TIMEOUT			 0x11
#define PXENV_STATUS_UDP_CLOSED				 0x18
#define PXENV_STATUS_UDP_OPEN				 0x19
#define PXENV_STATUS_TFTP_CLOSED			 0x1a
#define PXENV_STATUS_TFTP_OPEN				 0x1b
#define PXENV_STATUS_MCOPY_PROBLEM			 0x20
#define PXENV_STATUS_BIS_INTEGRITY_FAILURE		 0x21
#define PXENV_STATUS_BIS_VALIDATE_FAILURE		 0x22
#define PXENV_STATUS_BIS_INIT_FAILURE			 0x23
#define PXENV_STATUS_BIS_SHUTDOWN_FAILURE		 0x24
#define PXENV_STATUS_BIS_GBOA_FAILURE			 0x25
#define PXENV_STATUS_BIS_FREE_FAILURE			 0x26
#define PXENV_STATUS_BIS_GSI_FAILURE			 0x27
#define PXENV_STATUS_BIS_BAD_CKSUM			 0x28
#define PXENV_STATUS_TFTP_CANNOT_ARP_ADDRESS		 0x30
#define PXENV_STATUS_TFTP_OPEN_TIMEOUT			 0x32

#define PXENV_STATUS_TFTP_UNKNOWN_OPCODE		 0x33
#define PXENV_STATUS_TFTP_READ_TIMEOUT			 0x35
#define PXENV_STATUS_TFTP_ERROR_OPCODE			 0x36
#define PXENV_STATUS_TFTP_CANNOT_OPEN_CONNECTION	 0x38
#define PXENV_STATUS_TFTP_CANNOT_READ_FROM_CONNECTION	 0x39
#define PXENV_STATUS_TFTP_TOO_MANY_PACKAGES		 0x3a
#define PXENV_STATUS_TFTP_FILE_NOT_FOUND		 0x3b
#define PXENV_STATUS_TFTP_ACCESS_VIOLATION		 0x3c
#define PXENV_STATUS_TFTP_NO_MCAST_ADDRESS		 0x3d
#define PXENV_STATUS_TFTP_NO_FILESIZE			 0x3e
#define PXENV_STATUS_TFTP_INVALID_PACKET_SIZE		 0x3f
#define PXENV_STATUS_DHCP_TIMEOUT			 0x51
#define PXENV_STATUS_DHCP_NO_IP_ADDRESS			 0x52
#define PXENV_STATUS_DHCP_NO_BOOTFILE_NAME		 0x53
#define PXENV_STATUS_DHCP_BAD_IP_ADDRESS		 0x54
#define PXENV_STATUS_UNDI_INVALID_FUNCTION		 0x60
#define PXENV_STATUS_UNDI_MEDIATEST_FAILED		 0x61
#define PXENV_STATUS_UNDI_CANNOT_INIT_NIC_FOR_MCAST	 0x62
#define PXENV_STATUS_UNDI_CANNOT_INITIALIZE_NIC		 0x63
#define PXENV_STATUS_UNDI_CANNOT_INITIALIZE_PHY		 0x64
#define PXENV_STATUS_UNDI_CANNOT_READ_CONFIG_DATA	 0x65
#define PXENV_STATUS_UNDI_CANNOT_READ_INIT_DATA		 0x66
#define PXENV_STATUS_UNDI_BAD_MAC_ADDRESS		 0x67
#define PXENV_STATUS_UNDI_BAD_EEPROM_CHECKSUM		 0x68
#define PXENV_STATUS_UNDI_ERROR_SETTING_ISR		 0x69
#define PXENV_STATUS_UNDI_INVALID_STATE			 0x6a
#define PXENV_STATUS_UNDI_TRANSMIT_ERROR		 0x6b
#define PXENV_STATUS_UNDI_INVALID_PARAMETER		 0x6c
#define PXENV_STATUS_BSTRAP_PROMPT_MENU			 0x74
#define PXENV_STATUS_BSTRAP_MCAST_ADDR			 0x76
#define PXENV_STATUS_BSTRAP_MISSING_LIST		 0x77
#define PXENV_STATUS_BSTRAP_NO_RESPONSE			 0x78
#define PXENV_STATUS_BSTRAP_FILE_TOO_BIG		 0x79
#define PXENV_STATUS_BINL_CANCELED_BY_KEYSTROKE		 0xa0
#define PXENV_STATUS_BINL_NO_PXE_SERVER			 0xa1
#define PXENV_STATUS_NOT_AVAILABLE_IN_PMODE		 0xa2
#define PXENV_STATUS_NOT_AVAILABLE_IN_RMODE		 0xa3
#define PXENV_STATUS_BUSD_DEVICE_NOT_SUPPORTED		 0xb0
#define PXENV_STATUS_LOADER_NO_FREE_BASE_MEMORY		 0xc0
#define PXENV_STATUS_LOADER_NO_BC_ROMID			 0xc1
#define PXENV_STATUS_LOADER_BAD_BC_ROMID		 0xc2
#define PXENV_STATUS_LOADER_BAD_BC_RUNTIME_IMAGE	 0xc3
#define PXENV_STATUS_LOADER_NO_UNDI_ROMID		 0xc4
#define PXENV_STATUS_LOADER_BAD_UNDI_ROMID		 0xc5
#define PXENV_STATUS_LOADER_BAD_UNDI_DRIVER_IMAGE	 0xc6
#define PXENV_STATUS_LOADER_NO_PXE_STRUCT		 0xc8
#define PXENV_STATUS_LOADER_NO_PXENV_STRUCT		 0xc9
#define PXENV_STATUS_LOADER_UNDI_START			 0xca
#define PXENV_STATUS_LOADER_BC_START			 0xcb




/*
 * some other defines 
 */
#define PKTBUF_SIZE        (65536 / MAX_OPEN)

#define TFTP_BLOCKSIZE_LG2 9
#define TFTP_BLOCKSIZE     (1 << TFTP_BLOCKSIZE_LG2)
#define PKTBUF_SEG 0x4000
#define DNS_MAX_SERVERS    4

#define is_digit(c) (((c) >= '0') && ((c) <= '9'))
#define htons(x)    ( ( ((x) & 0xff) << 8) + ( ((x) &0xff00) >> 8) )
#define ntohs(x)    htons(x)
#define htonl(x)    ( ( ((x) & 0xff) << 24)     + ( ((x) & 0xff00) << 8 ) + \
                      ( ((x) & 0xff0000) >> 8 ) + ( ((x) & 0xff000000) >> 24) )
#define ntohl(x)    htonl(x)

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


/*
 * structures 
 */
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

struct open_file_t {
    uint16_t tftp_localport;   /* Local port number  (0=not in us)*/
    uint16_t tftp_remoteport;  /* Remote port number */
    uint32_t tftp_remoteip;    /* Remote IP address */
    uint32_t tftp_filepos;     /* bytes downloaded (includeing buffer) */
    uint32_t tftp_filesize;    /* Total file size(*) */
    uint32_t tftp_blksize;     /* Block size for this connection(*) */
    uint16_t tftp_bytesleft;   /* Unclaimed data bytes */
    uint16_t tftp_lastpkt;     /* Sequence number of last packet (NBO) */
    uint16_t tftp_dataptr;     /* Pointer to available data */
    uint8_t  tftp_goteof;      /* 1 if the EOF packet received */
    uint8_t  tftp_unused;      /* Currently unused */
    /* These values are preinitialized and not zeroed on close */
    uint16_t tftp_nextport;    /* Next port number for this slot (HBO) */
    uint16_t tftp_pktbuf;      /* Packet buffer offset */
} __attribute__ ((packed));

struct pxe_udp_write_pkt {
    uint16_t status;
    uint32_t sip;
    uint32_t gip;
    uint16_t lport;
    uint16_t rport;
    uint16_t buffersize;
    uint16_t buffer[2];
} __attribute__ ((packed));

struct pxe_udp_read_pkt {
    uint16_t status;
    uint32_t sip;
    uint32_t dip;
    uint16_t rport;
    uint16_t lport;
    uint16_t buffersize;
    uint16_t buffer[2];
} __attribute__ ((packed));

struct pxe_bootp_query_pkt {
    uint16_t status;
    uint16_t packettype;
    uint16_t buffersize;
    uint16_t buffer[2];
    uint16_t bufferlimit;
} __attribute__ ((packed));

struct pxe_udp_open_pkt {
    uint16_t status;
    uint32_t sip;
} __attribute__ ((packed));

struct gpxe_file_api_check {
    uint16_t status;
    uint16_t size;
    uint32_t magic;
    uint32_t provider;
    uint32_t apimask;
    uint32_t flags;
} __attribute__ ((packed));

struct gpxe_file_open {
    uint16_t status;
    uint16_t filehandle;
    uint16_t filename[2];
    uint32_t reserved;
} __attribute__ ((packed));

struct gpxe_get_file_size {
    uint16_t status;
    uint16_t filehandle;
    uint32_t filesize;
} __attribute__ ((packed));

struct gpxe_file_read {
    uint16_t status;
    uint16_t filehandle;
    uint16_t buffersize;
    uint16_t buffer[2];
} __attribute__ ((packed));



/*
 * functions
 */
int ip_ok(uint32_t);
void parse_dhcp(int);
void parse_dhcp_options(void *, int, int);




#endif /* pxe.h */
