#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#define SYS_LIGHTWEIGHT_PROT	1
#define LWIP_NETIF_API		1
#define LWIP_DNS		1

#define DNS_TABLE_SIZE		16
#define DNS_MAX_SERVERS		4
#define TCP_WND			32768
#define TCP_MSS			4096
#define TCP_SND_BUF		4096

#endif /* __LWIPOPTS_H__ */
