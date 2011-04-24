#include <minmax.h>
#include <lwip/api.h>
#include "pxe.h"
#include "url.h"
#include "tftp.h"

const uint8_t TimeoutTable[] = {
    2, 2, 3, 3, 4, 5, 6, 7, 9, 10, 12, 15, 18, 21, 26, 31, 37, 44,
    53, 64, 77, 92, 110, 132, 159, 191, 229, 255, 255, 255, 255, 0
};
struct tftp_options {
    const char *str_ptr;        /* string pointer */
    size_t      offset;		/* offset into socket structre */
};

#define IFIELD(x)	offsetof(struct inode, x)
#define PFIELD(x)	(offsetof(struct inode, pvt) + \
			 offsetof(struct pxe_pvt_inode, x))

static const struct tftp_options tftp_options[] =
{
    { "tsize",   IFIELD(size) },
    { "blksize", PFIELD(tftp_blksize) },
};
static const int tftp_nopts = sizeof tftp_options / sizeof tftp_options[0];

static void tftp_error(struct inode *file, uint16_t errnum,
		       const char *errstr);

static void tftp_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    if (socket->tftp_localport != 0) {
	tftp_error(inode, 0, "No error, file close");
    }
    if (socket->conn) {
	netconn_delete(socket->conn);
	socket->conn = NULL;
    }
    free(socket->tftp_pktbuf);
}

/**
 * Send an ERROR packet.  This is used to terminate a connection.
 *
 * @inode:	Inode structure
 * @errnum:	Error number (network byte order)
 * @errstr:	Error string (included in packet)
 */
static void tftp_error(struct inode *inode, uint16_t errnum,
		       const char *errstr)
{
    static struct {
	uint16_t err_op;
	uint16_t err_num;
	char err_msg[64];
    } __packed err_buf;
    struct netbuf *nbuf;
    int len = min(strlen(errstr), sizeof(err_buf.err_msg)-1);
    struct pxe_pvt_inode *socket = PVT(inode);

    err_buf.err_op  = TFTP_ERROR;
    err_buf.err_num = errnum;
    memcpy(err_buf.err_msg, errstr, len);
    err_buf.err_msg[len] = '\0';

    nbuf = netbuf_new();
    netbuf_ref(nbuf, &err_buf, 4 + len + 1);
    netconn_send(socket->conn, nbuf);
    /* If something goes wrong, there is nothing we can do, anyway... */
    netbuf_delete(nbuf);
}

/**
 * Send ACK packet. This is a common operation and so is worth canning.
 *
 * @param: inode,   Inode pointer
 * @param: ack_num, Packet # to ack (network byte order)
 *
 */
static void ack_packet(struct inode *inode, uint16_t ack_num)
{
    err_t err;
    static uint16_t ack_packet_buf[2];
    struct netbuf *nbuf;
    struct pxe_pvt_inode *socket = PVT(inode);

    /* Packet number to ack */
    ack_packet_buf[0]     = TFTP_ACK;
    ack_packet_buf[1]     = ack_num;

    nbuf = netbuf_new();
    netbuf_ref(nbuf, ack_packet_buf, 4);
    err = netconn_send(socket->conn, nbuf);
    netbuf_delete(nbuf);
    (void)err;
#if 0
    printf("sent %s\n", err ? "FAILED" : "OK");
#endif
}

/*
 * Get a fresh packet if the buffer is drained, and we haven't hit
 * EOF yet.  The buffer should be filled immediately after draining!
 */
static void tftp_get_packet(struct inode *inode)
{
    int last_pkt;
    const uint8_t *timeout_ptr;
    uint8_t timeout;
    uint16_t buffersize;
    jiffies_t oldtime;
    void *data = NULL;
    struct netbuf *nbuf;
    u16_t nbuf_len;
    struct pxe_pvt_inode *socket = PVT(inode);

    /*
     * Start by ACKing the previous packet; this should cause
     * the next packet to be sent.
     */
    timeout_ptr = TimeoutTable;
    timeout = *timeout_ptr++;
    oldtime = jiffies();

 ack_again:
    ack_packet(inode, socket->tftp_lastpkt);

    while (timeout) {
	nbuf = netconn_recv(socket->conn);
	if (!nbuf) {
	    jiffies_t now = jiffies();

	    if (now-oldtime >= timeout) {
		oldtime = now;
		timeout = *timeout_ptr++;
		if (!timeout)
		    break;
		goto ack_again;
	    }
            continue;
	}

	netbuf_first(nbuf);
	nbuf_len = 0;
	nbuf_len = netbuf_len(nbuf);
	if (nbuf_len <= PKTBUF_SIZE)
	    netbuf_copy(nbuf, packet_buf, nbuf_len);
	else
	    nbuf_len = 0; /* impossible mtu < PKTBUF_SIZE */
	netbuf_delete(nbuf);
	if (nbuf_len < 4)  /* Bad size for a DATA packet */
	    continue;

        data = packet_buf;
        if (*(uint16_t *)data != TFTP_DATA)    /* Not a data packet */
            continue;

        /* If goes here, recevie OK, break */
        break;
    }

    /* time runs out */
    if (timeout == 0)
	kaboom();

    last_pkt = socket->tftp_lastpkt;
    last_pkt = ntohs(last_pkt);       /* Host byte order */
    last_pkt++;
    last_pkt = htons(last_pkt);       /* Network byte order */
    if (*(uint16_t *)(data + 2) != last_pkt) {
        /*
         * Wrong packet, ACK the packet and try again.
         * This is presumably because the ACK got lost,
         * so the server just resent the previous packet.
         */
#if 0
	printf("Wrong packet, wanted %04x, got %04x\n", \
               htons(last_pkt), htons(*(uint16_t *)(data+2)));
#endif
        goto ack_again;
    }

    /* It's the packet we want.  We're also EOF if the size < blocksize */
    socket->tftp_lastpkt = last_pkt;    /* Update last packet number */
    buffersize = nbuf_len - 4;		/* Skip TFTP header */
    memcpy(socket->tftp_pktbuf, packet_buf + 4, buffersize);
    socket->tftp_dataptr = socket->tftp_pktbuf;
    socket->tftp_filepos += buffersize;
    socket->tftp_bytesleft = buffersize;
    if (buffersize < socket->tftp_blksize) {
        /* it's the last block, ACK packet immediately */
        ack_packet(inode, *(uint16_t *)(data + 2));

        /* Make sure we know we are at end of file */
        inode->size 		= socket->tftp_filepos;
        socket->tftp_goteof	= 1;
        tftp_close_file(inode);
    }
}

/**
 * Open a TFTP connection to the server
 *
 * @param:inode, the inode to store our state in
 * @param:ip, the ip to contact to get the file
 * @param:filename, the file we wanna open
 *
 * @out: open_file_t structure, stores in file->open_file
 * @out: the lenght of this file, stores in file->file_len
 *
 */
void tftp_open(struct url_info *url, struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    char *buf;
    struct netbuf *nbuf;
    u16_t nbuf_len;
    char *p;
    char *options;
    char *data;
    static const char rrq_tail[] = "octet\0""tsize\0""0\0""blksize\0""1408";
    static __lowmem char rrq_packet_buf[2+2*FILENAME_MAX+sizeof rrq_tail];
    const struct tftp_options *tftp_opt;
    int i = 0;
    int err;
    int buffersize;
    int rrq_len;
    const uint8_t  *timeout_ptr;
    jiffies_t timeout;
    jiffies_t oldtime;
    uint16_t opcode;
    uint16_t blk_num;
    uint32_t opdata, *opdata_ptr;
    struct ip_addr addr;

    if (url->type != URL_OLD_TFTP) {
	/*
	 * The TFTP URL specification allows the TFTP to end with a
	 * ;mode= which we just ignore.
	 */
	url_unescape(url->path, ';');
    }
    if (!url->port)
	url->port = TFTP_PORT;

    socket->fill_buffer = tftp_get_packet;
    socket->close = tftp_close_file;

    socket->conn = netconn_new(NETCONN_UDP);
    if (!socket->conn)
	return;

    socket->conn->recv_timeout = 15; /* A 15 ms recv timeout... */
    err = netconn_bind(socket->conn, NULL, ntohs(socket->tftp_localport));
    if (err) {
	printf("netconn_bind error %d\n", err);
	return;
    }

    buf = rrq_packet_buf;
    *(uint16_t *)buf = TFTP_RRQ;  /* TFTP opcode */
    buf += 2;

    buf = stpcpy(buf, url->path);

    buf++;			/* Point *past* the final NULL */
    memcpy(buf, rrq_tail, sizeof rrq_tail);
    buf += sizeof rrq_tail;

    rrq_len = buf - rrq_packet_buf;

    timeout_ptr = TimeoutTable;   /* Reset timeout */
    
sendreq:
    netconn_disconnect(socket->conn);
    timeout = *timeout_ptr++;
    if (!timeout)
	return;			/* No file available... */
    oldtime = jiffies();

    nbuf = netbuf_new();
    netbuf_ref(nbuf, rrq_packet_buf, rrq_len);
    addr.addr =  socket->tftp_remoteip = url->ip;
    netconn_sendto(socket->conn, nbuf, &addr, url->port);
    netbuf_delete(nbuf);

    /* If the WRITE call fails, we let the timeout take care of it... */
wait_pkt:
    netconn_disconnect(socket->conn);
    for (;;) {
	nbuf = netconn_recv(socket->conn);
	if (!nbuf) {
	    jiffies_t now = jiffies();
	    if (now - oldtime >= timeout)
		 goto sendreq;
	} else {
	    /* Make sure the packet actually came from the server */
	    bool ok_source;
	    ok_source = netbuf_fromaddr(nbuf)->addr == socket->tftp_remoteip;
	    nbuf_len = netbuf_len(nbuf);
	    if (nbuf_len <= PKTBUF_SIZE)
		netbuf_copy(nbuf, packet_buf, nbuf_len);
	    else
		nbuf_len = 0; /* impossible mtu < PKTBUF_SIZE */
	    netbuf_delete(nbuf);
	    if (ok_source)
	        break;
	}
    }

    socket->tftp_remoteport = htons(netbuf_fromport(nbuf));
    netconn_connect(socket->conn, netbuf_fromaddr(nbuf), netbuf_fromport(nbuf));

    /* filesize <- -1 == unknown */
    inode->size = -1;
    socket->tftp_blksize = TFTP_BLOCKSIZE;
    buffersize = nbuf_len - 2;	  /* bytes after opcode */
    if (buffersize < 0)
        goto wait_pkt;                     /* Garbled reply */

    /*
     * Get the opcode type, and parse it
     */
    opcode = *(uint16_t *)packet_buf;
    switch (opcode) {
    case TFTP_ERROR:
        inode->size = 0;
        break;			/* ERROR reply; don't try again */

    case TFTP_DATA:
        /*
         * If the server doesn't support any options, we'll get a
         * DATA reply instead of OACK. Stash the data in the file
         * buffer and go with the default value for all options...
         *
         * We got a DATA packet, meaning no options are
         * suported. Save the data away and consider the
         * length undefined, *unless* this is the only
         * data packet...
         */
        buffersize -= 2;
        if (buffersize < 0)
            goto wait_pkt;
        data = packet_buf + 2;
        blk_num = *(uint16_t *)data;
        data += 2;
        if (blk_num != htons(1))
            goto wait_pkt;
        socket->tftp_lastpkt = blk_num;
        if (buffersize > TFTP_BLOCKSIZE)
            goto err_reply;	/* Corrupt */

	socket->tftp_pktbuf = malloc(TFTP_BLOCKSIZE);
	if (!socket->tftp_pktbuf)
	    goto err_reply;	/* Internal error */

        if (buffersize < TFTP_BLOCKSIZE) {
            /*
             * This is the final EOF packet, already...
             * We know the filesize, but we also want to
             * ack the packet and set the EOF flag.
             */
            inode->size = buffersize;
            socket->tftp_goteof = 1;
            ack_packet(inode, blk_num);
        }

        socket->tftp_bytesleft = buffersize;
        socket->tftp_dataptr = socket->tftp_pktbuf;
        memcpy(socket->tftp_pktbuf, data, buffersize);
	break;

    case TFTP_OACK:
        /*
         * Now we need to parse the OACK packet to get the transfer
         * and packet sizes.
         */

        options = packet_buf + 2;
	p = options;

	while (buffersize) {
	    const char *opt = p;

	    /*
	     * If we find an option which starts with a NUL byte,
	     * (a null option), we're either seeing garbage that some
	     * TFTP servers add to the end of the packet, or we have
	     * no clue how to parse the rest of the packet (what is
	     * an option name and what is a value?)  In either case,
	     * discard the rest.
	     */
	    if (!*opt)
		goto done;

            while (buffersize) {
                if (!*p)
                    break;	/* Found a final null */
                *p++ |= 0x20;
		buffersize--;
            }
	    if (!buffersize)
		break;		/* Unterminated option */

	    /* Consume the terminal null */
	    p++;
	    buffersize--;

	    if (!buffersize)
		break;		/* No option data */

            /*
             * Parse option pointed to by options; guaranteed to be
	     * null-terminated
             */
            tftp_opt = tftp_options;
            for (i = 0; i < tftp_nopts; i++) {
                if (!strcmp(opt, tftp_opt->str_ptr))
                    break;
                tftp_opt++;
            }
            if (i == tftp_nopts)
                goto err_reply; /* Non-negotitated option returned,
				   no idea what it means ...*/

            /* get the address of the filed that we want to write on */
            opdata_ptr = (uint32_t *)((char *)inode + tftp_opt->offset);
	    opdata = 0;

            /* do convert a number-string to decimal number, just like atoi */
            while (buffersize--) {
		uint8_t d = *p++;
                if (d == '\0')
                    break;              /* found a final null */
		d -= '0';
                if (d > 9)
                    goto err_reply;     /* Not a decimal digit */
                opdata = opdata*10 + d;
            }
	    *opdata_ptr = opdata;
	}

	/* Parsing successful, allocate buffer */
	socket->tftp_pktbuf = malloc(socket->tftp_blksize);
	if (!socket->tftp_pktbuf)
	    goto err_reply;
	break;

    default:
	printf("TFTP unknown opcode %d\n", ntohs(opcode));
	goto err_reply;
    }

err_reply:
    /* Build the TFTP error packet */
    tftp_error(inode, TFTP_EOPTNEG, "TFTP protocol error");
    inode->size = 0;

done:
    if (!inode->size) {
	free(socket->tftp_pktbuf);
	netconn_delete(socket->conn);
	socket->conn = NULL;
    }
    return;

}
