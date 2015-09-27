#include <syslinux/pxe_api.h>
#include <com32.h>
#include <core.h>
#include <net.h>
#include <pxe.h>
#include <minmax.h>

/* Common receive buffer */
static __lowmem char packet_buf[PKTBUF_SIZE] __aligned(16);

extern uint16_t get_port(void);
extern void free_port(uint16_t);

const struct url_scheme url_schemes[] = {
    { "tftp", tftp_open, 0 },
    { NULL, NULL, 0 }
};

/**
 * Open a socket
 *
 * @param:socket, the socket to open
 *
 * @out: error code, 0 on success, -1 on failure
 */
int core_udp_open(struct pxe_pvt_inode *socket __unused)
{
    struct net_private_tftp *priv = &socket->net.tftp;

    /* Allocate local UDP port number */
    priv->localport = get_port();

    return 0;
}

/**
 * Close a socket
 *
 * @param:socket, the socket to open
 */
void core_udp_close(struct pxe_pvt_inode *socket)
{
    struct net_private_tftp *priv = &socket->net.tftp;

    if (priv->localport)
	free_port(priv->localport);
}

/**
 * Establish a connection on an open socket
 *
 * @param:socket, the open socket
 * @param:ip, the ip address
 * @param:port, the port number, host-byte order
 */
void core_udp_connect(struct pxe_pvt_inode *socket, uint32_t ip,
		      uint16_t port)
{
    struct net_private_tftp *priv = &socket->net.tftp;

    socket->tftp_remoteport = htons(port);
    priv->remoteip = ip;

}

/**
 * Tear down a connection on an open socket
 *
 * @param:socket, the open socket
 */
void core_udp_disconnect(struct pxe_pvt_inode *socket __unused)
{
}

/**
 * Read data from the network stack
 *
 * @param:socket, the open socket
 * @param:buf, location of buffer to store data
 * @param:buf_len, size of buffer

 * @out: src_ip, ip address of the data source
 * @out: src_port, port number of the data source, host-byte order
 */
int core_udp_recv(struct pxe_pvt_inode *socket, void *buf, uint16_t *buf_len,
		  uint32_t *src_ip, uint16_t *src_port)
{
    static __lowmem struct s_PXENV_UDP_READ  udp_read;
    struct net_private_tftp *priv = &socket->net.tftp;
    uint16_t bytes;
    int err;

    udp_read.status      = 0;
    udp_read.buffer      = FAR_PTR(packet_buf);
    udp_read.buffer_size = PKTBUF_SIZE;
    udp_read.dest_ip     = IPInfo.myip;
    udp_read.d_port      = priv->localport;

    err = pxe_call(PXENV_UDP_READ, &udp_read);
    if (err)
	return err;

    if (udp_read.status)
	return udp_read.status;

    bytes = min(udp_read.buffer_size, *buf_len);
    memcpy(buf, packet_buf, bytes);

    *src_ip = udp_read.src_ip;
    *src_port = ntohs(udp_read.s_port);
    *buf_len = bytes;

    return 0;
}

/**
 * Send a UDP packet.
 *
 * @param:socket, the open socket
 * @param:data, data buffer to send
 * @param:len, size of data bufer
 */
void core_udp_send(struct pxe_pvt_inode *socket, const void *data, size_t len)
{
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    struct net_private_tftp *priv = &socket->net.tftp;
    void *lbuf;
    uint16_t tid;

    lbuf = lmalloc(len);
    if (!lbuf)
	return;

    memcpy(lbuf, data, len);

    tid = priv->localport;   /* TID(local port No) */
    udp_write.buffer    = FAR_PTR(lbuf);
    udp_write.ip        = priv->remoteip;
    udp_write.gw        = gateway(udp_write.ip);
    udp_write.src_port  = tid;
    udp_write.dst_port  = socket->tftp_remoteport;
    udp_write.buffer_size = len;

    pxe_call(PXENV_UDP_WRITE, &udp_write);

    lfree(lbuf);
}

/**
 * Send a UDP packet to a destination
 *
 * @param:socket, the open socket
 * @param:data, data buffer to send
 * @param:len, size of data bufer
 * @param:ip, the ip address
 * @param:port, the port number, host-byte order
 */
void core_udp_sendto(struct pxe_pvt_inode *socket, const void *data, size_t len,
		     uint32_t ip, uint16_t port)
{
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    struct net_private_tftp *priv = &socket->net.tftp;
    void *lbuf;
    uint16_t tid;

    lbuf = lmalloc(len);
    if (!lbuf)
	return;

    memcpy(lbuf, data, len);

    tid = priv->localport;   /* TID(local port No) */
    udp_write.buffer    = FAR_PTR(lbuf);
    udp_write.ip        = ip;
    udp_write.gw        = gateway(udp_write.ip);
    udp_write.src_port  = tid;
    udp_write.dst_port  = htons(port);
    udp_write.buffer_size = len;

    pxe_call(PXENV_UDP_WRITE, &udp_write);

    lfree(lbuf);
}


/**
 * Network stack-specific initialization
 *
 * Initialize UDP stack
 */
void net_core_init(void)
{
    int err;
    static __lowmem struct s_PXENV_UDP_OPEN udp_open;
    udp_open.src_ip = IPInfo.myip;
    err = pxe_call(PXENV_UDP_OPEN, &udp_open);
    if (err || udp_open.status) {
        printf("Failed to initialize UDP stack ");
        printf("%d\n", udp_open.status);
	kaboom();
    }
}

void probe_undi(void)
{
}

void pxe_init_isr(void)
{
}

int reset_pxe(void)
{
    static __lowmem struct s_PXENV_UDP_CLOSE udp_close;
    int err = 0;

    pxe_idle_cleanup();

    pxe_call(PXENV_UDP_CLOSE, &udp_close);

    return err;
}

#if GPXE

static void gpxe_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    static __lowmem struct s_PXENV_FILE_CLOSE file_close;

    file_close.FileHandle = socket->tftp_remoteport;
    pxe_call(PXENV_FILE_CLOSE, &file_close);
}

/**
 * Get a fresh packet from a gPXE socket
 * @param: inode -> Inode pointer
 *
 */
static void gpxe_get_packet(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    static __lowmem struct s_PXENV_FILE_READ file_read;
    int err;

    while (1) {
        file_read.FileHandle  = socket->tftp_remoteport;
        file_read.Buffer      = FAR_PTR(packet_buf);
        file_read.BufferSize  = PKTBUF_SIZE;
        err = pxe_call(PXENV_FILE_READ, &file_read);
        if (!err)  /* successed */
            break;

        if (file_read.Status != PXENV_STATUS_TFTP_OPEN)
	    kaboom();
    }

    memcpy(socket->tftp_pktbuf, packet_buf, file_read.BufferSize);

    socket->tftp_dataptr   = socket->tftp_pktbuf;
    socket->tftp_bytesleft = file_read.BufferSize;
    socket->tftp_filepos  += file_read.BufferSize;

    if (socket->tftp_bytesleft == 0)
        inode->size = socket->tftp_filepos;

    /* if we're done here, close the file */
    if (inode->size > socket->tftp_filepos)
        return;

    /* Got EOF, close it */
    socket->tftp_goteof = 1;
    gpxe_close_file(inode);
}

const struct pxe_conn_ops gpxe_conn_ops = {
    .fill_buffer	= gpxe_get_packet,
    .close		= gpxe_close_file,
};

/**
 * Open a url using gpxe
 *
 * @param:inode, the inode to store our state in
 * @param:url, the url we want to open
 *
 * @out: open_file_t structure, stores in file->open_file
 * @out: the lenght of this file, stores in file->file_len
 *
 */
void gpxe_open(struct inode *inode, const char *url)
{
    static __lowmem struct s_PXENV_FILE_OPEN file_open;
    static __lowmem struct s_PXENV_GET_FILE_SIZE file_size;
    static __lowmem char lowurl[2*FILENAME_MAX];
    struct pxe_pvt_inode *socket = PVT(inode);
    int err;

    socket->tftp_pktbuf = malloc(PKTBUF_SIZE);
    if (!socket->tftp_pktbuf)
	return;

    snprintf(lowurl, sizeof lowurl, "%s", url);
    file_open.Status        = PXENV_STATUS_BAD_FUNC;
    file_open.FileName      = FAR_PTR(lowurl);
    err = pxe_call(PXENV_FILE_OPEN, &file_open);
    if (err)
	return;


    socket->ops = &gpxe_conn_ops;
    socket->tftp_remoteport = file_open.FileHandle;
    file_size.Status        = PXENV_STATUS_BAD_FUNC;
    file_size.FileHandle = file_open.FileHandle;
    err = pxe_call(PXENV_GET_FILE_SIZE, &file_size);
    if (err) {
	inode->size = -1; /* fallback size; this shouldn't be an error */
    } else {
	inode->size = file_size.FileSize;
    }
}

#endif /* GPXE */
