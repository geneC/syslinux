#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define SYS_LIGHTWEIGHT_PROT	1
#define LWIP_NETIF_API		1
#define LWIP_DNS		1

#define MEM_SIZE		(1 << 18)
#if 0
#define MEMP_OVERFLOW_CHECK	2 /* XXX: for debugging */
#define MEMP_SANITY_CHECK	1 /* XXX: for debugging */
#else
#define MEMP_OVERFLOW_CHECK	0 /* XXX: for debugging */
#define MEMP_SANITY_CHECK	0 /* XXX: for debugging */
#endif
#define MEM_USE_POOLS_TRY_BIGGER_POOL	1

#define TCPIP_MBOX_SIZE         	512
#define TCPIP_THREAD_PRIO		0
#define TCPIP_THREAD_STACKSIZE		32768

#define DEFAULT_UDP_RECVMBOX_SIZE	16
#define DEFAULT_TCP_RECVMBOX_SIZE	128
#define DEFAULT_ACCEPTMBOX_SIZE		4

#define LWIP_SOCKET			0

#define MEM_LIBC_MALLOC			0
#define MEMP_MEM_MALLOC			0

#define MEMP_NUM_TCP_PCB		64
#define MEMP_NUM_TCP_SEG		256
#define MEMP_NUM_REASSDATA		32
#define MEMP_NUM_SYS_TIMEOUT		8
#define MEMP_NUM_NETCONN		64
#define MEMP_NUM_TCPIP_MSG_API		64
#define MEMP_NUM_TCPIP_MSG_INPKT	64
#define PBUF_POOL_SIZE			256
#define ARP_TABLE_SIZE			16
#define IP_REASS_MAX_PBUFS		64
#define IP_REASS_MAXAGE			10

#define LWIP_NETIF_API		1

#define LWIP_DNS		1
#define DNS_TABLE_SIZE		16
#define DNS_MAX_SERVERS		4
#define TCP_WND			0xff00
#define TCP_MSS			1460
#define TCP_SND_BUF		4096

#endif /* __LWIPOPTS_H__ */
