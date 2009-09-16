/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include <core.h>

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"

#include <inttypes.h>
#include <string.h>
#include <syslinux/pxe_api.h>
#include <core.h>
#include <dprintf.h>

int pxe_call(int, void *);
#define PKTBUF_SIZE	2048

/* Define those to better describe your network interface. */
#define IFNAME0 'u'
#define IFNAME1 'n'

static struct netif undi_netif;

/**
 * In this function, the hardware should be initialized.
 * Called from undiif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this undiif
 */
static void
low_level_init(struct netif *netif)
{
  static __lowmem t_PXENV_UNDI_GET_INFORMATION undi_info;
  /* struct undiif *undiif = netif->state; */

  pxe_call(PXENV_UNDI_GET_INFORMATION, &undi_info);

  /* set MAC hardware address length */
  netif->hwaddr_len = undi_info.HwAddrLen;

  /* set MAC hardware address */
  memcpy(netif->hwaddr, undi_info.CurrentNodeAddress, undi_info.HwAddrLen);

  /* maximum transfer unit */
  netif->mtu = undi_info.MaxTranUnit;

  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  /* Do whatever else is needed to initialize interface. */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this undiif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

/*
 * XXX: This assumes an Ethernet media type.
 */
static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pxe_xmit {
    t_PXENV_UNDI_TRANSMIT xmit;
    t_PXENV_UNDI_TBD tbd;
  };
  static __lowmem struct pxe_xmit pxe;
  static __lowmem char pkt_buf[PKTBUF_SIZE];
  char *r;
  /* struct undiif *undiif = netif->state; */
  struct pbuf *q;
  static char eth_broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  (void)netif;

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  r = pkt_buf;
  for (q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    memcpy(r, q->payload, q->len);
    r += q->len;
  }

  do {
      memset(&pxe, 0, sizeof pxe);

      pxe.xmit.Protocol = 0;	/* XXX: P_UNKNOWN: MAC layer */
      pxe.xmit.XmitFlag = !memcmp(pkt_buf, eth_broadcast, sizeof eth_broadcast);
      pxe.xmit.DestAddr = FAR_PTR(pkt_buf);
      pxe.xmit.TBD = FAR_PTR(&pxe.tbd);
      pxe.tbd.ImmedLength = r - pkt_buf;
      pxe.tbd.Xmit = FAR_PTR(pkt_buf);
      pxe.tbd.DataBlkCount = 0;

      pxe_call(PXENV_UNDI_TRANSMIT, &pxe.xmit);
  } while (pxe.xmit.Status == PXENV_STATUS_OUT_OF_RESOURCES);

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

static void get_packet_fragment(t_PXENV_UNDI_ISR *isr)
{
  do {
    isr->FuncFlag = PXENV_UNDI_ISR_IN_GET_NEXT;
    pxe_call(PXENV_UNDI_ISR, &isr);
  } while (isr->FuncFlag != PXENV_UNDI_ISR_OUT_RECEIVE);
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this undiif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(t_PXENV_UNDI_ISR *isr)
{
  struct pbuf *p, *q;
  const char *r;
  int len;

  /* Obtain the size of the packet and put it into the "len"
     variable. */
  len = isr->FrameLength;

#if ETH_PAD_SIZE
  len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);

  if (p != NULL) {
#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /*
     * We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf.
     */
    r = GET_PTR(isr->Frame);
    for (q = p; q != NULL; q = q->next) {
      /*
       * Read enough bytes to fill this pbuf in the chain. The
       * available data in the pbuf is given by the q->len
       * variable.
       */
      char *s = q->payload;
      int ql = q->len;

      while (ql) {
	int qb = isr->BufferLength < ql ? isr->BufferLength : ql;

	if (!qb) {
	  /*
	   * Only received a partial frame, must get the next one...
	   */
	  get_packet_fragment(isr);
	  r = GET_PTR(isr->Frame);
	} else {
	  memcpy(s, r, qb);
	  s += qb;
	  r += qb;
	  ql -= qb;
	}
      }
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);
  } else {
    /*
     * Dropped packet: we really should make sure we drain any partial
     * frame here...
     */
    while ((len -= isr->BufferLength) > 0)
      get_packet_fragment(isr);

    LINK_STATS_INC(link.memerr);
    LINK_STATS_INC(link.drop);
  }

  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this undiif
 */
void undiif_input(t_PXENV_UNDI_ISR *isr)
{
  struct eth_hdr *ethhdr;
  struct pbuf *p;

  /* move received packet into a new pbuf */
  p = low_level_input(isr);
  /* no packet could be read, silently ignore this */
  if (p == NULL) return;
  /* points to packet payload, which starts with an Ethernet header */
  ethhdr = p->payload;

  switch (htons(ethhdr->type)) {
  /* IP or ARP packet? */
  case ETHTYPE_IP:
  case ETHTYPE_ARP:
#if PPPOE_SUPPORT
  /* PPPoE packet? */
  case ETHTYPE_PPPOEDISC:
  case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
    /* full packet send to tcpip_thread to process */
    if (undi_netif.input(p, &undi_netif)!=ERR_OK)
     { LWIP_DEBUGF(NETIF_DEBUG, ("undiif_input: IP input error\n"));
       pbuf_free(p);
       p = NULL;
     }
    break;

  default:
    pbuf_free(p);
    p = NULL;
    break;
  }
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this undiif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
extern uint8_t pxe_irq_vector;
extern void pxe_isr(void);

/* XXX: move this somewhere sensible */
static void install_irq_vector(uint8_t irq, void (*isr)(void))
{
  unsigned int vec;

  if (irq < 8)
    vec = irq + 0x08;
  else if (irq < 16)
    vec = (irq - 8) + 0x70;
  else
    return;			/* ERROR */
  
  *(uint32_t *)(vec << 2) = (uint32_t)isr;
}

static err_t
undiif_init(struct netif *netif)
{
  static __lowmem t_PXENV_UNDI_GET_INFORMATION undi_info;

  LWIP_ASSERT("netif != NULL", (netif != NULL));
#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "undi";
#endif /* LWIP_NETIF_HOSTNAME */

  pxe_call(PXENV_UNDI_GET_INFORMATION, &undi_info);

  dprintf("UNDI: baseio %04x int %d MTU %d type %d\n",
	  undi_info.BaseIo, undi_info.IntNumber, undi_info.MaxTranUnit,
	  undi_info.HwType);

  /* Install the interrupt vector */
  pxe_irq_vector = undi_info.IntNumber;
  install_irq_vector(pxe_irq_vector, pxe_isr);

  /*
   * Initialize the snmp variables and counters inside the struct netif.
   * The last argument should be replaced with your link speed, in units
   * of bits per second.
   */
  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

  netif->state   = NULL;	/* Private pointer if we need it */
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

err_t undi_tcpip_start(struct ip_addr *ipaddr,
		       struct ip_addr *netmask,
		       struct ip_addr *gw)
{
  // Start the TCP/IP thread & init stuff
  tcpip_init(NULL, NULL);

  // This should be done *after* the threading system and receive thread
  // have both been started.
  return netifapi_netif_add(&undi_netif, ipaddr, netmask, gw, NULL,
			    undiif_init, ethernet_input);
}
