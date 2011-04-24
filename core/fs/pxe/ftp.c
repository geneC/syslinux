/*
 * ftp.c
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>
#include <minmax.h>
#include <sys/cpu.h>
#include <netinet/in.h>
#include <lwip/api.h>
#include "pxe.h"
#include "thread.h"
#include "strbuf.h"
#include "url.h"

static int ftp_cmd_response(struct inode *inode, const char *cmd,
			    const char *cmd_arg,
			    uint8_t *pasv_data, int *pn_ptr)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    int c;
    int pos, code;
    int pb, pn;
    bool ps;
    bool first_line, done;
    err_t err;
    char cmd_buf[4096];
    int cmd_len;
    const char *p;
    char *q;

    if (cmd) {
	cmd_len = strlcpy(cmd_buf, cmd, sizeof cmd_buf);
	if (cmd_len >= sizeof cmd_buf - 3)
	    return -1;
	q = cmd_buf + cmd_len;

	if (cmd_arg) {
	    p = cmd_arg;

	    *q++ = ' ';
	    cmd_len++;
	    while (*p) {
		if (++cmd_len < sizeof cmd_buf) *q++ = *p;
		if (*p == '\r')
		    if (++cmd_len < sizeof cmd_buf) *q++ = '\0';
		p++;
	    }

	    if (cmd_len >= sizeof cmd_buf - 2)
		return -1;
	}

	*q++ = '\r';
	*q++ = '\n';
	cmd_len += 2;

	err = netconn_write(socket->conn, cmd_buf, cmd_len, NETCONN_COPY);
	if (err)
	    return -1;
    }

    pos = code = pn = pb = 0;
    ps = false;
    first_line = true;
    done = false;

    while ((c = pxe_getc(inode)) >= 0) {
	//printf("%c", c);

	if (c == '\n') {
	    pos = 0;
	    if (done) {
		if (pn) {
		    pn += ps;
		    if (pn_ptr)
			*pn_ptr = pn;
		}
		//printf("FTP response: %u\n", code);
		return code;
	    }
	    first_line = false;
	    continue;
	}

	switch (pos++) {
	case 0:
	case 1:
	case 2:
	    if (c < '0' || c > '9') {
		if (first_line)
		    return -1;
		else
		    pos = 4;	/* Skip this line */
	    } else {
		code = (code*10) + (c - '0');
	    }
	    break;

	case 3:
	    pn = pb = 0;
	    ps = false;
	    if (c == ' ')
		done = true;
	    else if (c == '-')
		done = false;
	    else if (first_line)
		return -1;
	    else
		done = false;
	    break;

	default:
	    if (pasv_data) {
		if (c >= '0' && c <= '9') {
		    pb = (pb*10) + (c-'0');
		    if (pn < 6)
			pasv_data[pn] = pb;
		    ps = true;
		} else if (c == ',') {
		    pn++;
		    pb = 0;
		    ps = false;
		} else if (pn) {
		    pn += ps;
		    if (pn_ptr)
			*pn_ptr = pn;
		    pn = pb = 0;
		    ps = false;
		}
	    }
	    break;
	}
    }

    return -1;
}

static void ftp_free(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);

    if (socket->ctl) {
	tcp_close_file(socket->ctl);
	free_socket(socket->ctl);
	socket->ctl = NULL;
    }
    tcp_close_file(inode);
}

static void ftp_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket  = PVT(inode);
    struct pxe_pvt_inode *ctlsock;
    int resp;

    //printf("In ftp_close_file\n");

    ctlsock = socket->ctl ? PVT(socket->ctl) : NULL;
    if (ctlsock->conn) {
	resp = ftp_cmd_response(socket->ctl, "QUIT", NULL, NULL, NULL);
	while (resp == 226) {
	    resp = ftp_cmd_response(socket->ctl, NULL, NULL, NULL, NULL);
	}
    }
    ftp_free(inode);
}

void ftp_open(struct url_info *url, struct inode *inode, const char **redir)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    struct pxe_pvt_inode *ctlsock;
    struct ip_addr addr;
    uint8_t pasv_data[6];
    int pasv_bytes;
    int resp;
    err_t err;

    (void)redir;		/* FTP does not redirect */
    inode->size = 0;

    if (!url->port)
	url->port = 21;

    url_unescape(url->path, 0);

    socket->fill_buffer = tcp_fill_buffer;
    socket->close = ftp_close_file;

    /* Allocate a socket for the control connection */
    socket->ctl = alloc_inode(inode->fs, 0, sizeof(struct pxe_pvt_inode));
    if (!socket->ctl)
	return;
    ctlsock = PVT(socket->ctl);
    ctlsock->fill_buffer = tcp_fill_buffer;
    ctlsock->close = tcp_close_file;

    ctlsock->conn = netconn_new(NETCONN_TCP);
    if (!ctlsock->conn)
	goto err_free;
    addr.addr = url->ip;
    err = netconn_connect(ctlsock->conn, &addr, url->port);
    if (err)
	goto err_delete;

    do {
	resp = ftp_cmd_response(socket->ctl, NULL, NULL, NULL, NULL);
    } while (resp == 120);
    if (resp != 220)
	goto err_disconnect;

    if (!url->user)
	url->user = "anonymous";
    if (!url->passwd)
	url->passwd = "pxelinux@";

    resp = ftp_cmd_response(socket->ctl, "USER", url->user, NULL, NULL);
    if (resp != 202 && resp != 230) {
	if (resp != 331)
	    goto err_disconnect;

	resp = ftp_cmd_response(socket->ctl, "PASS", url->passwd, NULL, NULL);
	if (resp != 230)
	    goto err_disconnect;
    }

    resp = ftp_cmd_response(socket->ctl, "TYPE", "I", NULL, NULL);
    if (resp != 200)
	goto err_disconnect;

    resp = ftp_cmd_response(socket->ctl, "PASV", NULL, pasv_data, &pasv_bytes);
    //printf("%u PASV %u bytes %u,%u,%u,%u,%u,%u\n",
    //resp, pasv_bytes, pasv_data[0], pasv_data[1], pasv_data[2],
    //pasv_data[3], pasv_data[4], pasv_data[5]);
    if (resp != 227 || pasv_bytes != 6)
	goto err_disconnect;

    socket->conn = netconn_new(NETCONN_TCP);
    if (!socket->conn)
	goto err_disconnect;
    err = netconn_connect(socket->conn, (struct ip_addr *)&pasv_data[0],
			  ntohs(*(uint16_t *)&pasv_data[4]));
    if (err)
	goto err_disconnect;

    resp = ftp_cmd_response(socket->ctl, "RETR", url->path, NULL, NULL);
    if (resp != 125 && resp != 150)
	goto err_disconnect;

    inode->size = -1;
    return;			/* Sucess! */

err_disconnect:
    if (ctlsock->conn)
	netconn_write(ctlsock->conn, "QUIT\r\n", 6, NETCONN_NOCOPY);
    if (socket->conn)
	netconn_delete(socket->conn);
    if (ctlsock->buf)
	netbuf_delete(ctlsock->buf);
err_delete:
    if (ctlsock->conn)
	netconn_delete(ctlsock->conn);
err_free:
    free_socket(socket->ctl);
}
