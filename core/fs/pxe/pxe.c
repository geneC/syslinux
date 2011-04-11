#include <dprintf.h>
#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>
#include <minmax.h>
#include <sys/cpu.h>
#include "pxe.h"

#define GPXE 1

static uint16_t real_base_mem;	   /* Amount of DOS memory after freeing */

uint8_t MAC[MAC_MAX];		   /* Actual MAC address */
uint8_t MAC_len;                   /* MAC address len */
uint8_t MAC_type;                  /* MAC address type */

char __bss16 BOOTIFStr[7+3*(MAC_MAX+1)];
#define MAC_str (BOOTIFStr+7)	/* The actual hardware address */
char __bss16 SYSUUIDStr[8+32+5];
#define UUID_str (SYSUUIDStr+8)	/* The actual UUID */

char boot_file[256];		   /* From DHCP */
char path_prefix[256];		   /* From DHCP */
char dot_quad_buf[16];

static bool has_gpxe;
static uint32_t gpxe_funcs;
bool have_uuid = false;

/* Common receive buffer */
static __lowmem char packet_buf[PKTBUF_SIZE] __aligned(16);

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

/*
 * Allocate a local UDP port structure and assign it a local port number.
 * Return the inode pointer if success, or null if failure
 */
static struct inode *allocate_socket(struct fs_info *fs)
{
    struct inode *inode = alloc_inode(fs, 0, sizeof(struct pxe_pvt_inode));

    if (!inode) {
	malloc_error("socket structure");
    } else {
	struct pxe_pvt_inode *socket = PVT(inode);
	socket->tftp_localport = get_port();
	inode->mode = DT_REG;	/* No other types relevant for PXE */
    }

    return inode;
}

static void free_socket(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);

    free_port(socket->tftp_localport);
    free_inode(inode);
}

#if GPXE
static void gpxe_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    static __lowmem struct s_PXENV_FILE_CLOSE file_close;

    file_close.FileHandle = socket->tftp_remoteport;
    pxe_call(PXENV_FILE_CLOSE, &file_close);
}
#endif

static void pxe_close_file(struct file *file)
{
    struct inode *inode = file->inode;
    struct pxe_pvt_inode *socket = PVT(inode);

    if (!socket->tftp_goteof) {
#if GPXE
	if (socket->tftp_localport == 0xffff) {
	    gpxe_close_file(inode);
	} else
#endif
	if (socket->tftp_localport != 0) {
	    tftp_error(inode, 0, "No error, file close");
	}
    }

    free_socket(inode);
}

/**
 * Take a nubmer of bytes in memory and convert to lower-case hxeadecimal
 *
 * @param: dst, output buffer
 * @param: src, input buffer
 * @param: count, number of bytes
 *
 */
static void lchexbytes(char *dst, const void *src, int count)
{
    uint8_t half;
    uint8_t c;
    const uint8_t *s = src;

    for(; count > 0; count--) {
        c = *s++;
        half   = ((c >> 4) & 0x0f) + '0';
        *dst++ = half > '9' ? (half + 'a' - '9' - 1) : half;

        half   = (c & 0x0f) + '0';
        *dst++ = half > '9' ? (half + 'a' - '9' - 1) : half;
    }
}

/*
 * just like the lchexbytes, except to upper-case
 *
 */
static void uchexbytes(char *dst, const void *src, int count)
{
    uint8_t half;
    uint8_t c;
    const uint8_t *s = src;

    for(; count > 0; count--) {
        c = *s++;
        half   = ((c >> 4) & 0x0f) + '0';
        *dst++ = half > '9' ? (half + 'A' - '9' - 1) : half;

        half   = (c & 0x0f) + '0';
        *dst++ = half > '9' ? (half + 'A' - '9' - 1) : half;
    }
}

/*
 * Parse a single hexadecimal byte, which must be complete (two
 * digits).  This is used in URL parsing.
 */
static int hexbyte(const char *p)
{
    if (!is_hex(p[0]) || !is_hex(p[1]))
	return -1;
    else
	return (hexval(p[0]) << 4) + hexval(p[1]);
}

/*
 * Tests an IP address in _ip_ for validity; return with 0 for bad, 1 for good.
 * We used to refuse class E, but class E addresses are likely to become
 * assignable unicast addresses in the near future.
 *
 */
bool ip_ok(uint32_t ip)
{
    uint8_t ip_hi = (uint8_t)ip; /* First octet of the ip address */

    if (ip == 0xffffffff ||	/* Refuse the all-ones address */
	ip_hi == 0 ||		/* Refuse network zero */
	ip_hi == 127 ||		/* Refuse the loopback network */
	(ip_hi & 240) == 224)	/* Refuse class D */
	return false;

    return true;
}


/*
 * Take an IP address (in network byte order) in _ip_ and
 * output a dotted quad string to _dst_, returns the length
 * of the dotted quad ip string.
 *
 */
static int gendotquad(char *dst, uint32_t ip)
{
    int part;
    int i = 0, j;
    char temp[4];
    char *p = dst;

    for (; i < 4; i++) {
        j = 0;
        part = ip & 0xff;
        do {
            temp[j++] = (part % 10) + '0';
        }while(part /= 10);
        for (; j > 0; j--)
            *p++ = temp[j-1];
        *p++ = '.';

        ip >>= 8;
    }
    /* drop the last dot '.' and zero-terminate string*/
    *(--p) = 0;

    return p - dst;
}

/*
 * parse the ip_str and return the ip address with *res.
 * return the the string address after the ip string
 *
 */
static const char *parse_dotquad(const char *ip_str, uint32_t *res)
{
    const char *p = ip_str;
    uint8_t part = 0;
    uint32_t ip = 0;
    int i;

    for (i = 0; i < 4; i++) {
        while (is_digit(*p)) {
            part = part * 10 + *p - '0';
            p++;
        }
        if (i != 3 && *p != '.')
            return NULL;

        ip = (ip << 8) | part;
        part = 0;
        p++;
    }
    p--;

    *res = htonl(ip);
    return p;
}

/*
 * the ASM pxenv function wrapper, return 1 if error, or 0
 *
 */
int pxe_call(int opcode, void *data)
{
    extern void pxenv(void);
    com32sys_t regs;

#if 0
    printf("pxe_call op %04x data %p\n", opcode, data);
#endif

    memset(&regs, 0, sizeof regs);
    regs.ebx.w[0] = opcode;
    regs.es       = SEG(data);
    regs.edi.w[0] = OFFS(data);
    call16(pxenv, &regs, &regs);

    return regs.eflags.l & EFLAGS_CF;  /* CF SET if fail */
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
    static __lowmem struct {
	uint16_t err_op;
	uint16_t err_num;
	char err_msg[64];
    } __packed err_buf;
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    int len = min(strlen(errstr), sizeof(err_buf.err_msg)-1);
    struct pxe_pvt_inode *socket = PVT(inode);

    err_buf.err_op  = TFTP_ERROR;
    err_buf.err_num = errnum;
    memcpy(err_buf.err_msg, errstr, len);
    err_buf.err_msg[len] = '\0';

    udp_write.src_port    = socket->tftp_localport;
    udp_write.dst_port    = socket->tftp_remoteport;
    udp_write.ip          = socket->tftp_remoteip;
    udp_write.gw          = gateway(udp_write.ip);
    udp_write.buffer      = FAR_PTR(&err_buf);
    udp_write.buffer_size = 4 + len + 1;

    /* If something goes wrong, there is nothing we can do, anyway... */
    pxe_call(PXENV_UDP_WRITE, &udp_write);
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
    int err;
    static __lowmem uint16_t ack_packet_buf[2];
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    struct pxe_pvt_inode *socket = PVT(inode);

    /* Packet number to ack */
    ack_packet_buf[0]     = TFTP_ACK;
    ack_packet_buf[1]     = ack_num;
    udp_write.src_port    = socket->tftp_localport;
    udp_write.dst_port    = socket->tftp_remoteport;
    udp_write.ip          = socket->tftp_remoteip;
    udp_write.gw          = gateway(udp_write.ip);
    udp_write.buffer      = FAR_PTR(ack_packet_buf);
    udp_write.buffer_size = 4;

    err = pxe_call(PXENV_UDP_WRITE, &udp_write);
    (void)err;
#if 0
    printf("sent %s\n", err ? "FAILED" : "OK");
#endif
}


/**
 * Get a DHCP packet from the PXE stack into the trackbuf
 *
 * @param:  type,  packet type
 * @return: buffer size
 *
 */
static int pxe_get_cached_info(int type)
{
    int err;
    static __lowmem struct s_PXENV_GET_CACHED_INFO get_cached_info;
    printf(" %02x", type);

    get_cached_info.Status      = 0;
    get_cached_info.PacketType  = type;
    get_cached_info.BufferSize  = 8192;
    get_cached_info.Buffer      = FAR_PTR(trackbuf);
    err = pxe_call(PXENV_GET_CACHED_INFO, &get_cached_info);
    if (err) {
        printf("PXE API call failed, error  %04x\n", err);
	kaboom();
    }

    return get_cached_info.BufferSize;
}


/*
 * Return the type of pathname passed.
 */
enum pxe_path_type {
    PXE_RELATIVE,		/* No :: or URL */
    PXE_HOMESERVER,		/* Starting with :: */
    PXE_TFTP,			/* host:: */
    PXE_URL_TFTP,		/* tftp:// */
    PXE_URL,			/* Absolute URL syntax */
};

static enum pxe_path_type pxe_path_type(const char *str)
{
    const char *p;
    
    p = str;

    while (1) {
	switch (*p) {
	case ':':
	    if (p[1] == ':') {
		if (p == str)
		    return PXE_HOMESERVER;
		else
		    return PXE_TFTP;
	    } else if (p > str && p[1] == '/' && p[2] == '/') {
		if (!strncasecmp(str, "tftp://", 7))
		    return PXE_URL_TFTP;
		else
		    return PXE_URL;
	    }

	    /* else fall through */
	case '/': case '!': case '@': case '#': case '%':
	case '^': case '&': case '*': case '(': case ')':
	case '[': case ']': case '{': case '}': case '\\':
	case '|': case '=': case '`': case '~': case '\'':
	case '\"': case ';': case '>': case '<': case '?':
	case '\0':
	    /* Any of these characters terminate the colon search */
	    return PXE_RELATIVE;
	default:
	    break;
	}
	p++;
    }
}

#if GPXE

/**
 * Get a fresh packet from a gPXE socket
 * @param: inode -> Inode pointer
 *
 */
static void get_packet_gpxe(struct inode *inode)
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
#endif /* GPXE */


/*
 * mangle a filename pointed to by _src_ into a buffer pointed
 * to by _dst_; ends on encountering any whitespace.
 *
 */
static void pxe_mangle_name(char *dst, const char *src)
{
    size_t len = FILENAME_MAX-1;

    while (len-- && not_whitespace(*src))
	*dst++ = *src++;

    *dst = '\0';
}

/*
 * Get a fresh packet if the buffer is drained, and we haven't hit
 * EOF yet.  The buffer should be filled immediately after draining!
 */
static void fill_buffer(struct inode *inode)
{
    int err;
    int last_pkt;
    const uint8_t *timeout_ptr;
    uint8_t timeout;
    uint16_t buffersize;
    uint32_t oldtime;
    void *data = NULL;
    static __lowmem struct s_PXENV_UDP_READ udp_read;
    struct pxe_pvt_inode *socket = PVT(inode);

    if (socket->tftp_bytesleft || socket->tftp_goteof)
        return;

#if GPXE
    if (socket->tftp_localport == 0xffff) {
        get_packet_gpxe(inode);
        return;
    }
#endif

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
        udp_read.buffer      = FAR_PTR(packet_buf);
        udp_read.buffer_size = PKTBUF_SIZE;
        udp_read.src_ip      = socket->tftp_remoteip;
        udp_read.dest_ip     = IPInfo.myip;
        udp_read.s_port      = socket->tftp_remoteport;
        udp_read.d_port      = socket->tftp_localport;
        err = pxe_call(PXENV_UDP_READ, &udp_read);
        if (err) {
	    uint32_t now = jiffies();

	    if (now-oldtime >= timeout) {
		oldtime = now;
		timeout = *timeout_ptr++;
		if (!timeout)
		    break;
	    }
            continue;
        }

        if (udp_read.buffer_size < 4)  /* Bad size for a DATA packet */
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
    buffersize = udp_read.buffer_size - 4;  /* Skip TFTP header */
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
    }
}


/**
 * getfssec: Get multiple clusters from a file, given the starting cluster.
 * In this case, get multiple blocks from a specific TCP connection.
 *
 * @param: fs, the fs_info structure address, in pxe, we don't use this.
 * @param: buf, buffer to store the read data
 * @param: openfile, TFTP socket pointer
 * @param: blocks, 512-byte block count; 0FFFFh = until end of file
 *
 * @return: the bytes read
 *
 */
static uint32_t pxe_getfssec(struct file *file, char *buf,
			     int blocks, bool *have_more)
{
    struct inode *inode = file->inode;
    struct pxe_pvt_inode *socket = PVT(inode);
    int count = blocks;
    int chunk;
    int bytes_read = 0;

    count <<= TFTP_BLOCKSIZE_LG2;
    while (count) {
        fill_buffer(inode); /* If we have no 'fresh' buffer, get it */
        if (!socket->tftp_bytesleft)
            break;

        chunk = count;
        if (chunk > socket->tftp_bytesleft)
            chunk = socket->tftp_bytesleft;
        socket->tftp_bytesleft -= chunk;
        memcpy(buf, socket->tftp_dataptr, chunk);
	socket->tftp_dataptr += chunk;
        buf += chunk;
        bytes_read += chunk;
        count -= chunk;
    }


    if (socket->tftp_bytesleft || (socket->tftp_filepos < inode->size)) {
	fill_buffer(inode);
        *have_more = 1;
    } else if (socket->tftp_goteof) {
        /*
         * The socket is closed and the buffer drained; the caller will
	 * call close_file and therefore free the socket.
         */
        *have_more = 0;
    }

    return bytes_read;
}

/**
 * Open a TFTP connection to the server
 *
 * @param:filename, the file we wanna open
 *
 * @out: open_file_t structure, stores in file->open_file
 * @out: the lenght of this file, stores in file->file_len
 *
 */
static void __pxe_searchdir(const char *filename, struct file *file);
extern uint16_t PXERetry;

static void pxe_searchdir(const char *filename, struct file *file)
{
    int i = PXERetry;

    do {
	dprintf("PXE: file = %p, retries left = %d: ", file, i);
	__pxe_searchdir(filename, file);
	dprintf("%s\n", file->inode ? "ok" : "failed");
    } while (!file->inode && i--);
}

static void __pxe_searchdir(const char *filename, struct file *file)
{
    struct fs_info *fs = file->fs;
    struct inode *inode;
    struct pxe_pvt_inode *socket;
    char *buf;
    const char *np;
    char *p;
    char *options;
    char *data;
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    static __lowmem struct s_PXENV_UDP_READ  udp_read;
    static __lowmem struct s_PXENV_FILE_OPEN file_open;
    static const char rrq_tail[] = "octet\0""tsize\0""0\0""blksize\0""1408";
    static __lowmem char rrq_packet_buf[2+2*FILENAME_MAX+sizeof rrq_tail];
    const struct tftp_options *tftp_opt;
    int i = 0;
    int err;
    int buffersize;
    int rrq_len;
    const uint8_t  *timeout_ptr;
    uint32_t timeout;
    uint32_t oldtime;
    uint16_t tid;
    uint16_t opcode;
    uint16_t blk_num;
    uint32_t ip = 0;
    uint32_t opdata, *opdata_ptr;
    enum pxe_path_type path_type;
    char fullpath[2*FILENAME_MAX];
    uint16_t server_port = TFTP_PORT;  /* TFTP server port */

    inode = file->inode = NULL;
	
    buf = rrq_packet_buf;
    *(uint16_t *)buf = TFTP_RRQ;  /* TFTP opcode */
    buf += 2;

    path_type = pxe_path_type(filename);
    if (path_type == PXE_RELATIVE) {
	snprintf(fullpath, sizeof fullpath, "%s%s", fs->cwd_name, filename);
	path_type = pxe_path_type(filename = fullpath);
    }

    switch (path_type) {
    case PXE_RELATIVE:		/* Really shouldn't happen... */
    case PXE_URL:
	buf = stpcpy(buf, filename);
	ip = IPInfo.serverip;	/* Default server */
	break;

    case PXE_HOMESERVER:
	buf = stpcpy(buf, filename+2);
	ip = IPInfo.serverip;
	break;

    case PXE_TFTP:
	np = strchr(filename, ':');
	buf = stpcpy(buf, np+2);
	if (parse_dotquad(filename, &ip) != np)
	    ip = dns_resolv(filename);
	break;

    case PXE_URL_TFTP:
	np = filename + 7;
	while (*np && *np != '/' && *np != ':')
	    np++;
	if (np > filename + 7) {
	    if (parse_dotquad(filename + 7, &ip) != np)
		ip = dns_resolv(filename + 7);
	}
	if (*np == ':') {
	    np++;
	    server_port = 0;
	    while (*np >= '0' && *np <= '9')
		server_port = server_port * 10 + *np++ - '0';
	    server_port = server_port ? htons(server_port) : TFTP_PORT;
	}
	if (*np == '/')
	    np++;		/* Do *NOT* eat more than one slash here... */
	/*
	 * The ; is because of a quirk in the TFTP URI spec (RFC
	 * 3617); it is to be followed by TFTP modes, which we just ignore.
	 */
	while (*np && *np != ';') {
	    int v;
	    if (*np == '%' && (v = hexbyte(np+1)) > 0) {
		*buf++ = v;
		np += 3;
	    } else {
		*buf++ = *np++;
	    }
	}
	*buf = '\0';
	break;
    }

    buf++;			/* Point *past* the final NULL */
    memcpy(buf, rrq_tail, sizeof rrq_tail);
    buf += sizeof rrq_tail;

    rrq_len = buf - rrq_packet_buf;

    inode = allocate_socket(fs);
    if (!inode)
	return;			/* Allocation failure */
    socket = PVT(inode);

#if GPXE
    if (path_type == PXE_URL) {
	if (has_gpxe) {
	    file_open.Status        = PXENV_STATUS_BAD_FUNC;
	    file_open.FileName      = FAR_PTR(rrq_packet_buf + 2);
	    err = pxe_call(PXENV_FILE_OPEN, &file_open);
	    if (err)
		goto done;
	    
	    socket->tftp_localport = -1;
	    socket->tftp_remoteport = file_open.FileHandle;
	    inode->size = -1;
	    goto done;
	} else {
	    static bool already = false;
	    if (!already) {
		printf("URL syntax, but gPXE extensions not detected, "
		       "trying plain TFTP...\n");
		already = true;
	    }
	}
    }
#endif /* GPXE */

    if (!ip)
	    goto done;		/* No server */

    timeout_ptr = TimeoutTable;   /* Reset timeout */
    
sendreq:
    timeout = *timeout_ptr++;
    if (!timeout)
	return;			/* No file available... */
    oldtime = jiffies();

    socket->tftp_remoteip = ip;
    tid = socket->tftp_localport;   /* TID(local port No) */
    udp_write.buffer    = FAR_PTR(rrq_packet_buf);
    udp_write.ip        = ip;
    udp_write.gw        = gateway(udp_write.ip);
    udp_write.src_port  = tid;
    udp_write.dst_port  = server_port;
    udp_write.buffer_size = rrq_len;
    pxe_call(PXENV_UDP_WRITE, &udp_write);

    /* If the WRITE call fails, we let the timeout take care of it... */

wait_pkt:
    for (;;) {
        buf                  = packet_buf;
	udp_read.status      = 0;
        udp_read.buffer      = FAR_PTR(buf);
        udp_read.buffer_size = PKTBUF_SIZE;
        udp_read.dest_ip     = IPInfo.myip;
        udp_read.d_port      = tid;
        err = pxe_call(PXENV_UDP_READ, &udp_read);
        if (err || udp_read.status) {
	    uint32_t now = jiffies();
	    if (now - oldtime >= timeout)
		goto sendreq;
        } else {
	    /* Make sure the packet actually came from the server */
	    if (udp_read.src_ip == socket->tftp_remoteip)
		break;
	}
    }

    socket->tftp_remoteport = udp_read.s_port;

    /* filesize <- -1 == unknown */
    inode->size = -1;
    /* Default blksize unless blksize option negotiated */
    socket->tftp_blksize = TFTP_BLOCKSIZE;
    buffersize = udp_read.buffer_size - 2;  /* bytes after opcode */
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
            goto err_reply;  /* Corrupt */
        else if (buffersize < TFTP_BLOCKSIZE) {
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
	break;

    default:
	printf("TFTP unknown opcode %d\n", ntohs(opcode));
	goto err_reply;
    }

done:
    if (!inode->size) {
        free_socket(inode);
	return;
    }
    file->inode = inode;
    return;

err_reply:
    /* Build the TFTP error packet */
    tftp_error(inode, TFTP_EOPTNEG, "TFTP protocol error");
    printf("TFTP server sent an incomprehesible reply\n");
    kaboom();
}


/*
 * Store standard filename prefix
 */
static void get_prefix(void)
{
    int len;
    char *p;
    char c;

    if (!(DHCPMagic & 0x04)) {
	/* No path prefix option, derive from boot file */

	strlcpy(path_prefix, boot_file, sizeof path_prefix);
	len = strlen(path_prefix);
	p = &path_prefix[len - 1];
	
	while (len--) {
	    c = *p--;
	    c |= 0x20;
	    
	    c = (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'z') ||
		(c == '.' || c == '-');
	    if (!c)
		break;
	};
	
	if (len < 0)
	    p --;
	
	*(p + 2) = 0;                /* Zero-terminate after delimiter */
    }

    printf("TFTP prefix: %s\n", path_prefix);
    chdir(path_prefix);
}

/*
 * realpath for PXE
 */
static size_t pxe_realpath(struct fs_info *fs, char *dst, const char *src,
			   size_t bufsize)
{
    enum pxe_path_type path_type = pxe_path_type(src);

    return snprintf(dst, bufsize, "%s%s",
		    path_type == PXE_RELATIVE ? fs->cwd_name : "", src);
}

/*
 * chdir for PXE
 */
static int pxe_chdir(struct fs_info *fs, const char *src)
{
    /* The cwd for PXE is just a text prefix */
    enum pxe_path_type path_type = pxe_path_type(src);

    if (path_type == PXE_RELATIVE)
	strlcat(fs->cwd_name, src, sizeof fs->cwd_name);
    else
	strlcpy(fs->cwd_name, src, sizeof fs->cwd_name);

    dprintf("cwd = \"%s\"\n", fs->cwd_name);
    return 0;
}

 /*
  * try to load a config file, if found, return 1, or return 0
  *
  */
static int try_load(char *config_name)
{
    com32sys_t regs;

    printf("Trying to load: %-50s  ", config_name);
    pxe_mangle_name(KernelName, config_name);

    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(KernelName, 0);
    call16(core_open, &regs, &regs);
    if (regs.eflags.l & EFLAGS_ZF) {
	strcpy(ConfigName, KernelName);
        printf("\r");
        return 0;
    } else {
        printf("ok\n");
        return 1;
    }
}


/* Load the config file, return 1 if failed, or 0 */
static int pxe_load_config(void)
{
    const char *cfgprefix = "pxelinux.cfg/";
    const char *default_str = "default";
    char *config_file;
    char *last;
    int tries = 8;

    get_prefix();
    if (DHCPMagic & 0x02) {
        /* We got a DHCP option, try it first */
	if (try_load(ConfigName))
	    return 0;
    }

    /*
     * Have to guess config file name ...
     */
    config_file = stpcpy(ConfigName, cfgprefix);

    /* Try loading by UUID */
    if (have_uuid) {
	strcpy(config_file, UUID_str);
	if (try_load(ConfigName))
            return 0;
    }

    /* Try loading by MAC address */
    strcpy(config_file, MAC_str);
    if (try_load(ConfigName))
        return 0;

    /* Nope, try hexadecimal IP prefixes... */
    uchexbytes(config_file, (uint8_t *)&IPInfo.myip, 4);
    last = &config_file[8];
    while (tries) {
        *last = '\0';        /* Zero-terminate string */
	if (try_load(ConfigName))
            return 0;
        last--;           /* Drop one character */
        tries--;
    };

    /* Final attempt: "default" string */
    strcpy(config_file, default_str);
    if (try_load(ConfigName))
        return 0;

    printf("%-68s\n", "Unable to locate configuration file");
    kaboom();
}

/*
 * Generate the bootif string.
 */
static void make_bootif_string(void)
{
    const uint8_t *src;
    char *dst = BOOTIFStr;
    int i;

    dst += sprintf(dst, "BOOTIF=%02x", MAC_type);
    src = MAC;
    for (i = MAC_len; i; i--)
	dst += sprintf(dst, "-%02x", *src++);
}
/*
 * Generate the SYSUUID string, if we have one...
 */
static void make_sysuuid_string(void)
{
    static const uint8_t uuid_dashes[] = {4, 2, 2, 2, 6, 0};
    const uint8_t *src = uuid;
    const uint8_t *uuid_ptr = uuid_dashes;
    char *dst;

    SYSUUIDStr[0] = '\0';	/* If nothing there... */

    /* Try loading by UUID */
    if (have_uuid) {
	dst = stpcpy(SYSUUIDStr, "SYSUUID=");

        while (*uuid_ptr) {
	    int len = *uuid_ptr;

            lchexbytes(dst, src, len);
            dst += len * 2;
            src += len;
            uuid_ptr++;
            *dst++ = '-';
        }
        /* Remove last dash and zero-terminate */
	*--dst = '\0';
    }
}

/*
 * Generate an ip=<client-ip>:<boot-server-ip>:<gw-ip>:<netmask>
 * option into IPOption based on a DHCP packet in trackbuf.
 *
 */
char __bss16 IPOption[3+4*16];

static void genipopt(void)
{
    char *p = IPOption;
    const uint32_t *v = &IPInfo.myip;
    int i;

    p = stpcpy(p, "ip=");

    for (i = 0; i < 4; i++) {
	p += gendotquad(p, *v++);
	*p++ = ':';
    }
    *--p = '\0';
}


/* Generate ip= option and print the ip adress */
static void ip_init(void)
{
    uint32_t ip = IPInfo.myip;

    genipopt();
    gendotquad(dot_quad_buf, ip);

    ip = ntohl(ip);
    printf("My IP address seems to be %08X %s\n", ip, dot_quad_buf);
}

/*
 * Print the IPAPPEND strings, in order
 */
extern const uint16_t IPAppends[];
extern const char numIPAppends[];

static void print_ipappend(void)
{
    size_t i;

    for (i = 0; i < (size_t)numIPAppends; i++) {
	const char *p = (const char *)(size_t)IPAppends[i];
	if (*p)
	    printf("%s\n", p);
    }
}

/*
 * Validity check on possible !PXE structure in buf
 * return 1 for success, 0 for failure.
 *
 */
static int is_pxe(const void *buf)
{
    const struct pxe_t *pxe = buf;
    const uint8_t *p = buf;
    int i = pxe->structlength;
    uint8_t sum = 0;

    if (i < sizeof(struct pxe_t) ||
	memcmp(pxe->signature, "!PXE", 4))
        return 0;

    while (i--)
        sum += *p++;

    return sum == 0;
}

/*
 * Just like is_pxe, it checks PXENV+ structure
 *
 */
static int is_pxenv(const void *buf)
{
    const struct pxenv_t *pxenv = buf;
    const uint8_t *p = buf;
    int i = pxenv->length;
    uint8_t sum = 0;

    /* The pxeptr field isn't present in old versions */
    if (i < offsetof(struct pxenv_t, pxeptr) ||
	memcmp(pxenv->signature, "PXENV+", 6))
        return 0;

    while (i--)
        sum += *p++;

    return sum == 0;
}



/*
 * memory_scan_for_pxe_struct:
 * memory_scan_for_pxenv_struct:
 *
 *	If none of the standard methods find the !PXE/PXENV+ structure,
 *	look for it by scanning memory.
 *
 *	return the corresponding pxe structure if found, or NULL;
 */
static const void *memory_scan(uintptr_t start, int (*func)(const void *))
{
    const char *ptr;

    /* Scan each 16 bytes of conventional memory before the VGA region */
    for (ptr = (const char *)start; ptr < (const char *)0xA0000; ptr += 16) {
        if (func(ptr))
            return ptr;		/* found it! */
	ptr += 16;
    }
    return NULL;
}

static const struct pxe_t *memory_scan_for_pxe_struct(void)
{
    extern uint16_t BIOS_fbm;  /* Starting segment */

    return memory_scan(BIOS_fbm << 10, is_pxe);
}

static const struct pxenv_t *memory_scan_for_pxenv_struct(void)
{
    return memory_scan(0x10000, is_pxenv);
}

/*
 * Find the !PXE structure; we search for the following, in order:
 *
 * a. !PXE structure as SS:[SP + 4]
 * b. PXENV+ structure at [ES:BX]
 * c. INT 1Ah AX=0x5650 -> PXENV+
 * d. Search memory for !PXE
 * e. Search memory for PXENV+
 *
 * If we find a PXENV+ structure, we try to find a !PXE structure from
 * if if the API version is 2.1 or later
 *
 */
static int pxe_init(bool quiet)
{
    extern void pxe_int1a(void);
    char plan = 'A';
    uint16_t seg, off;
    uint16_t code_seg, code_len;
    uint16_t data_seg, data_len;
    const char *base = GET_PTR(InitStack);
    com32sys_t regs;
    const char *type;
    const struct pxenv_t *pxenv;
    const struct pxe_t *pxe;

    /* Assume API version 2.1 */
    APIVer = 0x201;

    /* Plan A: !PXE structure as SS:[SP + 4] */
    off = *(const uint16_t *)(base + 48);
    seg = *(const uint16_t *)(base + 50);
    pxe = MK_PTR(seg, off);
    if (is_pxe(pxe))
        goto have_pxe;

    /* Plan B: PXENV+ structure at [ES:BX] */
    plan++;
    off = *(const uint16_t *)(base + 24);  /* Original BX */
    seg = *(const uint16_t *)(base + 4);   /* Original ES */
    pxenv = MK_PTR(seg, off);
    if (is_pxenv(pxenv))
        goto have_pxenv;

    /* Plan C: PXENV+ structure via INT 1Ah AX=5650h  */
    plan++;
    memset(&regs, 0, sizeof regs);
    regs.eax.w[0] = 0x5650;
    call16(pxe_int1a, &regs, &regs);
    if (!(regs.eflags.l & EFLAGS_CF) && (regs.eax.w[0] == 0x564e)) {
	pxenv = MK_PTR(regs.es, regs.ebx.w[0]);
        if (is_pxenv(pxenv))
            goto have_pxenv;
    }

    /* Plan D: !PXE memory scan */
    plan++;
    if ((pxe = memory_scan_for_pxe_struct()))
        goto have_pxe;

    /* Plan E: PXENV+ memory scan */
    plan++;
    if ((pxenv = memory_scan_for_pxenv_struct()))
        goto have_pxenv;

    /* Found nothing at all !! */
    if (!quiet)
	printf("No !PXE or PXENV+ API found; we're dead...\n");
    return -1;

 have_pxenv:
    APIVer = pxenv->version;
    if (!quiet)
	printf("Found PXENV+ structure\nPXE API version is %04x\n", APIVer);

    /* if the API version number is 0x0201 or higher, use the !PXE structure */
    if (APIVer >= 0x201) {
	if (pxenv->length >= sizeof(struct pxenv_t)) {
	    pxe = GET_PTR(pxenv->pxeptr);
	    if (is_pxe(pxe))
		goto have_pxe;
	    /*
	     * Nope, !PXE structure missing despite API 2.1+, or at least
	     * the pointer is missing. Do a last-ditch attempt to find it
	     */
	    if ((pxe = memory_scan_for_pxe_struct()))
		goto have_pxe;
	}
	APIVer = 0x200;		/* PXENV+ only, assume version 2.00 */
    }

    /* Otherwise, no dice, use PXENV+ structure */
    data_len = pxenv->undidatasize;
    data_seg = pxenv->undidataseg;
    code_len = pxenv->undicodesize;
    code_seg = pxenv->undicodeseg;
    PXEEntry = pxenv->rmentry;
    type = "PXENV+";
    goto have_entrypoint;

 have_pxe:
    data_len = pxe->seg[PXE_Seg_UNDIData].size;
    data_seg = pxe->seg[PXE_Seg_UNDIData].sel;
    code_len = pxe->seg[PXE_Seg_UNDICode].size;
    code_seg = pxe->seg[PXE_Seg_UNDICode].sel;
    PXEEntry = pxe->entrypointsp;
    type = "!PXE";

 have_entrypoint:
    if (!quiet) {
	printf("%s entry point found (we hope) at %04X:%04X via plan %c\n",
	       type, PXEEntry.seg, PXEEntry.offs, plan);
	printf("UNDI code segment at %04X len %04X\n", code_seg, code_len);
	printf("UNDI data segment at %04X len %04X\n", data_seg, data_len);
    }

    code_seg = code_seg + ((code_len + 15) >> 4);
    data_seg = data_seg + ((data_len + 15) >> 4);

    real_base_mem = max(code_seg, data_seg) >> 6; /* Convert to kilobytes */

    return 0;
}

/*
 * See if we have gPXE
 */
static void gpxe_init(void)
{
    int err;
    static __lowmem struct s_PXENV_FILE_API_CHECK api_check;

    if (APIVer >= 0x201) {
	api_check.Size = sizeof api_check;
	api_check.Magic = 0x91d447b2;
	err = pxe_call(PXENV_FILE_API_CHECK, &api_check);
	if (!err && api_check.Magic == 0xe9c17b20)
	    gpxe_funcs = api_check.APIMask;
    }

    /* Necessary functions for us to use the gPXE file API */
    has_gpxe = (~gpxe_funcs & 0x4b) == 0;
}

/*
 * Initialize UDP stack
 *
 */
static void udp_init(void)
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


/*
 * Network-specific initialization
 */
static void network_init(void)
{
    struct bootp_t *bp = (struct bootp_t *)trackbuf;
    int pkt_len;

    *LocalDomain = 0;   /* No LocalDomain received */

    /*
     * Get the DHCP client identifiers (query info 1)
     */
    printf("Getting cached packet ");
    pkt_len = pxe_get_cached_info(1);
    parse_dhcp(pkt_len);
    /*
     * We don't use flags from the request packet, so
     * this is a good time to initialize DHCPMagic...
     * Initialize it to 1 meaning we will accept options found;
     * in earlier versions of PXELINUX bit 0 was used to indicate
     * we have found option 208 with the appropriate magic number;
     * we no longer require that, but MAY want to re-introduce
     * it in the future for vendor encapsulated options.
     */
    *(char *)&DHCPMagic = 1;

    /*
     * Get the BOOTP/DHCP packet that brought us file (and an IP
     * address). This lives in the DHCPACK packet (query info 2)
     */
    pkt_len = pxe_get_cached_info(2);
    parse_dhcp(pkt_len);
    /*
     * Save away MAC address (assume this is in query info 2. If this
     * turns out to be problematic it might be better getting it from
     * the query info 1 packet
     */
    MAC_len = bp->hardlen > 16 ? 0 : bp->hardlen;
    MAC_type = bp->hardware;
    memcpy(MAC, bp->macaddr, MAC_len);

    /*
     * Get the boot file and other info. This lives in the CACHED_REPLY
     * packet (query info 3)
     */
    pkt_len = pxe_get_cached_info(3);
    parse_dhcp(pkt_len);
    printf("\n");

    make_bootif_string();
    make_sysuuid_string();
    ip_init();
    print_ipappend();

    /*
     * Check to see if we got any PXELINUX-specific DHCP options; in particular,
     * if we didn't get the magic enable, do not recognize any other options.
     */
    if ((DHCPMagic & 1) == 0)
        DHCPMagic = 0;

    udp_init();
}

/*
 * Initialize pxe fs
 *
 */
static int pxe_fs_init(struct fs_info *fs)
{
    (void)fs;    /* drop the compile warning message */

    /* This block size is actually arbitrary... */
    fs->sector_shift = fs->block_shift = TFTP_BLOCKSIZE_LG2;
    fs->sector_size  = fs->block_size  = 1 << TFTP_BLOCKSIZE_LG2;

    /* This block size is actually arbitrary... */
    fs->sector_shift = fs->block_shift = TFTP_BLOCKSIZE_LG2;
    fs->sector_size  = fs->block_size  = 1 << TFTP_BLOCKSIZE_LG2;

    /* Find the PXE stack */
    if (pxe_init(false))
	kaboom();

    /* See if we also have a gPXE stack */
    gpxe_init();

    /* Network-specific initialization */
    network_init();

    /* Initialize network-card-specific idle handling */
    pxe_idle_init();

    /* Our name for the root */
    strcpy(fs->cwd_name, "::");

    return 0;
}

/*
 * Look to see if we are on an EFI CSM system.  Some EFI
 * CSM systems put the BEV stack in low memory, which means
 * a return to the PXE stack will crash the system.  However,
 * INT 18h works reliably, so in that case hack the stack and
 * point the "return address" to an INT 18h instruction.
 *
 * Hack the stack instead of the much simpler "just invoke INT 18h
 * if we want to reset", so that chainloading other NBPs will work.
 *
 * This manipulates the real-mode InitStack directly.  It relies on this
 * *not* being a currently active stack, i.e. the former
 * USE_PXE_PROVIDED_STACK no longer works.
 *
 * XXX: Disable this until we can find a better way to discriminate
 * between BIOSes that are broken on BEV return and BIOSes which are
 * broken on INT 18h.  Keying on the EFI CSM turns out to cause more
 * problems than it solves.
 */
extern far_ptr_t InitStack;

struct efi_struct {
    uint32_t magic;
    uint8_t  csum;
    uint8_t  len;
} __attribute__((packed));
#define EFI_MAGIC (('$' << 24)+('E' << 16)+('F' << 8)+'I')

static inline bool is_efi(const struct efi_struct *efi)
{
    /*
     * We don't verify the checksum, because it seems some CSMs leave
     * it at zero, sigh...
     */
    return (efi->magic == EFI_MAGIC) && (efi->len >= 83);
}

static void install_int18_hack(void)
{
#if 0
    static const uint8_t int18_hack[] =
    {
	0xcd, 0x18,			/* int $0x18 */
	0xea, 0xf0, 0xff, 0x00, 0xf0,	/* ljmpw $0xf000,$0xfff0 */
	0xf4				/* hlt */
    };
    uint16_t *retcode;

    retcode = GET_PTR(*(far_ptr_t *)((char *)GET_PTR(InitStack) + 44));

    /* Don't do this if the return already points to int $0x18 */
    if (*retcode != 0x18cd) {
	uint32_t efi_ptr;
	bool efi = false;

	for (efi_ptr = 0xe0000 ; efi_ptr < 0x100000 ; efi_ptr += 16) {
	    if (is_efi((const struct efi_struct *)efi_ptr)) {
		efi = true;
		break;
	    }
	}

	if (efi) {
	    uint8_t *src = GET_PTR(InitStack);
	    uint8_t *dst = src - sizeof int18_hack;

	    memmove(dst, src, 52);
	    memcpy(dst+52, int18_hack, sizeof int18_hack);
	    InitStack.offs -= sizeof int18_hack;

	    /* Clobber the return address */
	    *(uint16_t *)(dst+44) = OFFS_WRT(dst+52, InitStack.seg);
	    *(uint16_t *)(dst+46) = InitStack.seg;
	}
    }
#endif
}

int reset_pxe(void)
{
    static __lowmem struct s_PXENV_UDP_CLOSE udp_close;
    extern void gpxe_unload(void);
    int err = 0;

    pxe_idle_cleanup();

    pxe_call(PXENV_UDP_CLOSE, &udp_close);

    if (gpxe_funcs & 0x80) {
	/* gPXE special unload implemented */
	call16(gpxe_unload, &zero_regs, NULL);

	/* Locate the actual vendor stack... */
	err = pxe_init(true);
    }

    install_int18_hack();
    return err;
}

/*
 * This function unloads the PXE and UNDI stacks and
 * unclaims the memory.
 */
void unload_pxe(void)
{
    /* PXE unload sequences */
    static const uint8_t new_api_unload[] = {
	PXENV_UNDI_SHUTDOWN, PXENV_UNLOAD_STACK, PXENV_STOP_UNDI, 0
    };
    static const uint8_t old_api_unload[] = {
	PXENV_UNDI_SHUTDOWN, PXENV_UNLOAD_STACK, PXENV_UNDI_CLEANUP, 0
    };

    unsigned int api;
    const uint8_t *api_ptr;
    int err;
    size_t int_addr;
    static __lowmem union {
	struct s_PXENV_UNDI_SHUTDOWN undi_shutdown;
	struct s_PXENV_UNLOAD_STACK unload_stack;
	struct s_PXENV_STOP_UNDI stop_undi;
	struct s_PXENV_UNDI_CLEANUP undi_cleanup;
	uint16_t Status;	/* All calls have this as the first member */
    } unload_call;

    dprintf("FBM before unload = %d\n", BIOS_fbm);

    err = reset_pxe();

    dprintf("FBM after reset_pxe = %d, err = %d\n", BIOS_fbm, err);

    /* If we want to keep PXE around, we still need to reset it */
    if (KeepPXE || err)
	return;

    dprintf("APIVer = %04x\n", APIVer);

    api_ptr = APIVer >= 0x0200 ? new_api_unload : old_api_unload;
    while((api = *api_ptr++)) {
	dprintf("PXE call %04x\n", api);
	memset(&unload_call, 0, sizeof unload_call);
	err = pxe_call(api, &unload_call);
	if (err || unload_call.Status != PXENV_STATUS_SUCCESS) {
	    dprintf("PXE unload API call %04x failed\n", api);
	    goto cant_free;
	}
    }

    api = 0xff00;
    if (real_base_mem <= BIOS_fbm) {  /* Sanity check */ 
	dprintf("FBM %d < real_base_mem %d\n", BIOS_fbm, real_base_mem);
	goto cant_free;
    }
    api++;

    /* Check that PXE actually unhooked the INT 0x1A chain */
    int_addr = (size_t)GET_PTR(*(far_ptr_t *)(4 * 0x1a));
    int_addr >>= 10;
    if (int_addr >= real_base_mem || int_addr < BIOS_fbm) {
	BIOS_fbm = real_base_mem;
	dprintf("FBM after unload_pxe = %d\n", BIOS_fbm);
	return;
    }

    dprintf("Can't free FBM, real_base_mem = %d, "
	    "FBM = %d, INT 1A = %08x (%d)\n",
	    real_base_mem, BIOS_fbm,
	    *(uint32_t *)(4 * 0x1a), int_addr);

cant_free:
    printf("Failed to free base memory error %04x-%08x (%d/%dK)\n",
	   api, *(uint32_t *)(4 * 0x1a), BIOS_fbm, real_base_mem);
    return;
}

const struct fs_ops pxe_fs_ops = {
    .fs_name       = "pxe",
    .fs_flags      = FS_NODEV,
    .fs_init       = pxe_fs_init,
    .searchdir     = pxe_searchdir,
    .chdir         = pxe_chdir,
    .realpath      = pxe_realpath,
    .getfssec      = pxe_getfssec,
    .close_file    = pxe_close_file,
    .mangle_name   = pxe_mangle_name,
    .load_config   = pxe_load_config,
};
