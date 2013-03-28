/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * tcp.c
 *
 * Common operations for TCP-based network protocols
 */

#include <lwip/api.h>
#include "pxe.h"
#include "../../../version.h"
#include "url.h"

void tcp_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);

    if (socket->net.lwip.conn) {
	netconn_delete(socket->net.lwip.conn);
	socket->net.lwip.conn = NULL;
    }
    if (socket->net.lwip.buf) {
	netbuf_delete(socket->net.lwip.buf);
        socket->net.lwip.buf = NULL;
    }
}

void tcp_fill_buffer(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    void *data;
    u16_t len;
    err_t err;

    /* Clean up or advance an inuse netbuf */
    if (socket->net.lwip.buf) {
	if (netbuf_next(socket->net.lwip.buf) < 0) {
	    netbuf_delete(socket->net.lwip.buf);
	    socket->net.lwip.buf = NULL;
	}
    }
    /* If needed get a new netbuf */
    if (!socket->net.lwip.buf) {
	err = netconn_recv(socket->net.lwip.conn, &(socket->net.lwip.buf));
	if (!socket->net.lwip.buf || err) {
	    socket->tftp_goteof = 1;
	    if (inode->size == -1)
		inode->size = socket->tftp_filepos;
	    socket->ops->close(inode);
	    return;
	}
    }
    /* Report the current fragment of the netbuf */
    err = netbuf_data(socket->net.lwip.buf, &data, &len);
    if (err) {
	printf("netbuf_data err: %d\n", err);
	kaboom();
    }
    socket->tftp_dataptr = data;
    socket->tftp_filepos += len;
    socket->tftp_bytesleft = len;
    return;
}

const struct pxe_conn_ops tcp_conn_ops = {
    .fill_buffer	= tcp_fill_buffer,
    .close		= tcp_close_file,
};
