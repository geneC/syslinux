#ifndef _NET_H
#define _NET_H

#include <stdint.h>
#include <stddef.h>

/* Protocol family */
enum net_core_proto {
    NET_CORE_TCP,
    NET_CORE_UDP,
};

void net_core_init(void);

struct pxe_pvt_inode;

int net_core_open(struct pxe_pvt_inode *socket, enum net_core_proto proto);
void net_core_close(struct pxe_pvt_inode *socket);

void net_core_connect(struct pxe_pvt_inode *socket,
		      uint32_t ip, uint16_t port);
void net_core_disconnect(struct pxe_pvt_inode *socket);

int net_core_recv(struct pxe_pvt_inode *socket, void *buf, uint16_t *buf_len,
		  uint32_t *src_ip, uint16_t *src_port);

void net_core_send(struct pxe_pvt_inode *socket,
		   const void *data, size_t len);

void probe_undi(void);
void pxe_init_isr(void);

#endif /* _NET_H */
