#include <stdio.h>
#include <string.h>
#include <core.h>
#include <fs.h>
#include <minmax.h>
#include <sys/cpu.h>
#include "pxe.h"

#define GPXE 1

uint32_t server_ip = 0;            /* IP address of boot server */
uint32_t net_mask = 0;             /* net_mask of this subnet */
uint32_t gate_way = 0;             /* Default router */
uint16_t server_port = TFTP_PORT;  /* TFTP server port */
uint16_t real_base_mem;            /* Amount of DOS memory after freeing */

char MAC_str[3 * (MAC_MAX + 1)];   /* MAC address as a string */
char MAC[MAC_MAX + 1];             /* Actual MAC address */
uint8_t MAC_len;                   /* MAC address len */
uint8_t MAC_type;                  /* MAC address type */

char boot_file[256];
char path_prefix[256];
char dot_quad_buf[16];

static struct open_file_t Files[MAX_OPEN];
static int has_gpxe;
static uint8_t uuid_dashes[] = {4, 2, 2, 2, 6, 0};
int have_uuid = 0;

const uint8_t TimeoutTable[] = {
    2, 2, 3, 3, 4, 5, 6, 7, 9, 10, 12, 15, 18, 21, 26, 31, 37, 44, 53, 64, 77,
    92, 110, 132, 159, 191, 229, 255, 255, 255, 255, 0
};

/* PXE unload sequences */
const uint8_t new_api_unload[] = {
    PXENV_UDP_CLOSE, PXENV_UNDI_SHUTDOWN, 
    PXENV_UNLOAD_STACK, PXENV_STOP_UNDI, 0
};
const uint8_t old_api_unload[] = {
    PXENV_UDP_CLOSE, PXENV_UNDI_SHUTDOWN,
    PXENV_UNLOAD_STACK, PXENV_UNDI_CLEANUP, 0
};

struct tftp_options {
    const char *str_ptr;        /* string pointer */
    size_t      offset;		/* offset into socket structre */
};
static const struct tftp_options tftp_options[] =
{
    { "tsize",   offsetof(struct open_file_t, tftp_filesize) },
    { "blksize", offsetof(struct open_file_t, tftp_blksize) },
};
static const int tftp_nopts = sizeof tftp_options / sizeof tftp_options[0];

static void tftp_error(struct open_file_t *file, uint16_t errnum,
		       const char *errstr);

/*
 * Initialize the Files structure
 */
static void files_init(void)
{
    int i;
    struct open_file_t *socket = Files;
    uint16_t pktbuf = 0;
    uint16_t nextport = 49152;

    for (i = 0; i < MAX_OPEN; i++) {
	socket->tftp_pktbuf = pktbuf;
	socket->tftp_nextport = nextport;
	pktbuf += PKTBUF_SIZE;
	nextport++;
	socket++;
    }
}

/*
 * Allocate a local UDP port structure.
 * return the socket pointer if success, or null if failure
 *    
 */
static struct open_file_t *allocate_socket(void)
{
    int i;
    struct open_file_t *socket = Files;
    uint16_t nextport;
    
    for (i = 0; i < MAX_OPEN; i++) {
        if (!socket->tftp_localport)
	    break;
        socket++;
    }
    
    if (i == MAX_OPEN)
	return NULL;

    /*
     * Allocate a socket number. Socket numbers are made guaranteed
     * unique by including the socket slot number; add a counter value
     * to keep the numbers from being likely to get immediately
     * reused.  The mask enforces wraparound to the range 49152-57343.
     */
    nextport = socket->tftp_nextport;
    socket->tftp_nextport = (nextport + (1 << MAX_OPEN_LG2)) & 0xdfff;
    socket->tftp_localport = htons(nextport); /* Socket now in use */
    return socket;
}

/*
 * free socket in _file_.
 */
static void free_socket(struct open_file_t *file)
{
    /* tftp_nextport and tftp_pktbuf are not cleared */
    memset(file, 0, offsetof(struct open_file_t, tftp_nextport));
}

static void pxe_close_file(struct file *file)
{
    struct open_file_t *open_file = file->open_file;

    if (open_file->tftp_localport && !open_file->tftp_goteof)
	tftp_error(open_file, 0, "No error, file close");

    free_socket(open_file);
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
 * Tests an IP address in _ip_ for validity; return with 0 for bad, 1 for good.
 * We used to refuse class E, but class E addresses are likely to become
 * assignable unicast addresses in the near future.
 *
 */
int ip_ok(uint32_t ip)
{
    if (ip == -1 ||            /* Refuse the all-one address */
        (ip & 0xff) == 0 ||    /* Refuse network zero */
        (ip & 0xff) == 0xff || /* Refuse loopback */
        (ip & 0xf0) == 0xe0 )  /* Refuse class D */
        return 0;

    return 1;
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
    int  i  = 0;
    uint8_t part = 0;
    uint32_t ip  = 0;
    
    for (; i < 4; i++) {
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
    p --;
    
    *res = ip;
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
 * @file:	TFTP file pointer
 * @errnum:	Error number (network byte order)
 * @errstr:	Error string (included in packet)
 */
static void tftp_error(struct open_file_t *file, uint16_t errnum,
		       const char *errstr)
{
    static __lowmem struct {
	uint16_t err_op;
	uint16_t err_num;
	char err_msg[64];
    } __packed err_buf;
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    int len = min(strlen(errstr), sizeof(err_buf.err_msg)-1);

    err_buf.err_op  = TFTP_ERROR;
    err_buf.err_num = errnum;
    memcpy(err_buf.err_msg, errstr, len);
    err_buf.err_msg[len] = '\0';
    
    udp_write.src_port    = file->tftp_localport;
    udp_write.dst_port    = file->tftp_remoteport;
    udp_write.ip          = file->tftp_remoteip;
    udp_write.gw          = ((udp_write.ip ^ MyIP) & net_mask) ? gate_way : 0;
    udp_write.buffer      = FAR_PTR(&err_buf);
    udp_write.buffer_size = 4 + len + 1;

    /* If something goes wrong, there is nothing we can do, anyway... */
    pxe_call(PXENV_UDP_WRITE, &udp_write);
}    


/**
 * Send ACK packet. This is a common operation and so is worth canning.
 *
 * @param: file,    TFTP block pointer
 * @param: ack_num, Packet # to ack (network byte order)
 * 
 */
static void ack_packet(struct open_file_t *file, uint16_t ack_num)
{
    int err;
    static __lowmem uint16_t ack_packet_buf[2];
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;    
 
    /* Packet number to ack */
    ack_packet_buf[0]     = TFTP_ACK;
    ack_packet_buf[1]     = ack_num;
    udp_write.src_port    = file->tftp_localport;
    udp_write.dst_port    = file->tftp_remoteport;
    udp_write.ip          = file->tftp_remoteip;
    udp_write.gw          = ((udp_write.ip ^ MyIP) & net_mask) ? gate_way : 0;
    udp_write.buffer      = FAR_PTR(ack_packet_buf);
    udp_write.buffer_size = 4;

    err = pxe_call(PXENV_UDP_WRITE, &udp_write);
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

  

#if GPXE

/*
 * Return 1 if and only if the buffer pointed to by
 * url is a URL (contains ://)
 *
 */
static int is_url(const char *url)
{
    while (*url) {
        if (!strncmp(url, "://", 3))
            return 1;
          
        url++;
    }    
    return 0;
}


/*
 * Return CF=0 if and only if the buffer pointed to by DS:SI is a URL 
 * (contains ://) *and* the gPXE extensions API is available. No 
 * registers modified.
 */
static int is_gpxe(char *url)
{
    int err;
    static __lowmem struct s_PXENV_FILE_API_CHECK api_check;
    char *gpxe_warning_msg = 
        "URL syntax, but gPXE extensions not detected, tring plain TFTP...\n";
    
    if (! is_url(url))
        return 0;
    
    api_check.Size  = sizeof api_check;
    api_check.Magic = 0x91d447b2;
    /* If has_gpxe is greater than one, means the gpxe status is unknow */
    while (has_gpxe > 1)  {
        err = pxe_call(PXENV_FILE_API_CHECK, &api_check);
        if (err || api_check.Magic != 0xe9c17b20) 
            printf("%s\n", gpxe_warning_msg);
        else             
            has_gpxe = (~api_check.Provider & 0xffff) & 0x4b ? 0 : 1;
        
        if (!has_gpxe)
            printf("%s\n", gpxe_warning_msg);
    }
    
    return has_gpxe == 1;
}

/**
 * Get a fresh packet from a gPXE socket
 * @param: file -> socket structure
 *
 */
static void get_packet_gpxe(struct open_file_t *file)
{
    static __lowmem struct s_PXENV_FILE_READ file_read;
    int err;
 
    while (1) {
        file_read.FileHandle  = file->tftp_remoteport;
        file_read.Buffer.offs = file->tftp_pktbuf;
        file_read.Buffer.seg  = PKTBUF_SEG;
        file_read.BufferSize  = PKTBUF_SIZE;
        err = pxe_call(PXENV_FILE_READ, &file_read);
        if (!err)  /* successed */
            break;

        if (file_read.Status != PXENV_STATUS_TFTP_OPEN)
	    kaboom();
    }

    file->tftp_bytesleft = file_read.BufferSize;
    file->tftp_filepos  += file_read.BufferSize;
    
    if (file->tftp_bytesleft == 0)
        file->tftp_filesize = file->tftp_filepos;
    
    /* if we're done here, close the file */
    if (file->tftp_filesize > file->tftp_filepos) 
        return;

    /* Got EOF, close it */
    file->tftp_goteof = 1;
    pxe_call(PXENV_FILE_CLOSE, &file_read);
}
#endif /* GPXE */


/*
 * mangle a filename pointed to by _src_ into a buffer pointed
 * to by _dst_; ends on encountering any whitespace.
 *
 * The first four bytes of the manged name is the IP address of
 * the download host, 0 for no host, or -1 for a gPXE URL.
 *
 */
static void pxe_mangle_name(char *dst, const char *src)
{
    const char *p = src;
    uint32_t ip = server_ip;
    int i = 0;
   
#if GPXE
    if (is_url(src)) {
        ip = -1;
        goto store;
    }
#endif

    if (*p == 0 || !(p = strstr(src, "::"))) {
        /* seems no ip, so make ip to 0 */
        p = src;
        ip = 0;
    } else if (p == src) {
        /* skip the first two-colon */
        p += 2;
    } else {
        /*
         * we have a :: prefix of some sort, it could be either a DNS
         * name or dot-quad IP address. Try the dot-quad first.
         */
        p = src;
        if ((p = parse_dotquad(p, &ip)) && !strncmp(p, "::", 2)) {
            p += 2;
        } else {
            ip = dns_resolv(p);
            if (ip && (p = strchr(p, ':')) && p[1] == ':') {
                p += 2;
            } else {
                /* no ip, too */
                p = src;
                ip = 0;
            }
        }
    }
  
 store:
    *(uint32_t *)dst = ip;  
    dst += 4;
    i = FILENAME_MAX - 5;

    do {
	if (!not_whitespace(*p))
	    break;
	*dst++ = *p++;
    } while (i--);

    i++;
    while (i) {
        *dst++ = 0;
        i--;
    }
}


/*
 * Does the opposite of mangle_name; converts a DOS-mangled
 * filename to the conventional representation.  This is 
 * needed for the BOOT_IMAGE= parameter for the kernel.
 */
static char *pxe_unmangle_name(char *dst, const char *src)
{
    uint32_t ip = *(uint32_t *)src;
    int ip_len = 0;
   
    if (ip != 0 && ip != -1) {
        ip_len = gendotquad(dst, *(uint32_t *)src);
        dst += ip_len;
    }
    src += 4;
    return stpcpy(dst, src);
}
       
/*
 * Get a fresh packet if the buffer is drained, and we haven't hit
 * EOF yet.  The buffer should be filled immediately after draining!
 */
static void fill_buffer(struct open_file_t *file)
{
    int err;
    int last_pkt;
    const uint8_t *timeout_ptr = TimeoutTable;
    uint8_t timeout;
    uint16_t buffersize;
    uint32_t oldtime;
    void *data = NULL;
    static __lowmem struct s_PXENV_UDP_READ udp_read;
        
    if (file->tftp_bytesleft || file->tftp_goteof)
        return;

#if GPXE
    if (file->tftp_localport == 0xffff) {
        get_packet_gpxe(file);
        return;
    }
#endif


    /*
     * Start by ACKing the previous packet; this should cause
     * the next packet to be sent.
     */
 ack_again:   
    ack_packet(file, file->tftp_lastpkt);
    
    timeout_ptr = TimeoutTable;    
    timeout = *timeout_ptr++;
    oldtime = jiffies();
    while (timeout) {
        udp_read.buffer.offs = file->tftp_pktbuf;
        udp_read.buffer.seg  = PKTBUF_SEG;
        udp_read.buffer_size = PKTBUF_SIZE;
        udp_read.src_ip      = file->tftp_remoteip;
        udp_read.dest_ip     = MyIP;
        udp_read.s_port      = file->tftp_remoteport;
        udp_read.d_port      = file->tftp_localport;
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

        data = MK_PTR(PKTBUF_SEG, file->tftp_pktbuf);
        if (*(uint16_t *)data != TFTP_DATA)    /* Not a data packet */
            continue;
                
        /* If goes here, recevie OK, break */
        break;
    }

    /* time runs out */
    if (timeout == 0)
	kaboom();
    
    last_pkt = file->tftp_lastpkt;
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
    file->tftp_lastpkt = last_pkt;    /* Update last packet number */
    buffersize = udp_read.buffer_size - 4;  /* Skip TFTP header */
    file->tftp_dataptr = file->tftp_pktbuf + 4;
    file->tftp_filepos += buffersize;
    file->tftp_bytesleft = buffersize;
    if (buffersize < file->tftp_blksize) {
        /* it's the last block, ACK packet immediately */
        ack_packet(file, *(uint16_t *)(data + 2));

        /* Make sure we know we are at end of file */
        file->tftp_filesize = file->tftp_filepos;
        file->tftp_goteof   = 1;
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
static uint32_t pxe_getfssec(struct file *gfile, char *buf, 
			     int blocks, bool *have_more)
{
    struct open_file_t *file = gfile->open_file;
    int count = blocks;
    int chunk;
    int bytes_read = 0;

    count <<= TFTP_BLOCKSIZE_LG2;
    while (count) {
        fill_buffer(file);         /* If we have no 'fresh' buffer, get it */
        if (!file->tftp_bytesleft)
            break;
        
        chunk = count;
        if (chunk > file->tftp_bytesleft)
            chunk = file->tftp_bytesleft;
        file->tftp_bytesleft -= chunk;
        memcpy(buf, MK_PTR(PKTBUF_SEG, file->tftp_dataptr), chunk);
	file->tftp_dataptr += chunk;
        buf += chunk;
        bytes_read += chunk;
        count -= chunk;
    }

    
    if (file->tftp_bytesleft || (file->tftp_filepos < file->tftp_filesize)) {
	fill_buffer(file);
        *have_more = 1;
    } else if (file->tftp_goteof) {
        /* 
         * The socket is closed and the buffer drained; the caller will
	 * call close_file and therefore free the socket.
         */
        *have_more = 0;
    }
    
    return bytes_read;
 }

 

/*
 * Fill the packet tail with the tftp informations then retures the lenght
 */
static int fill_tail(char *dst)
{
    static const char tail[] = "octet\0""tsize\0""0\0""blksize\0""1408";
    
    memcpy(dst, tail, sizeof tail);
    return sizeof tail;
}


/**
 * Open a TFTP connection to the server
 *
 * @param:filename, the file we wanna open
 *
 * @out: open_file_t structure, stores in file->open_file
 * @ouT: the lenght of this file, stores in file->file_len
 *
 */
static void pxe_searchdir(char *filename, struct file *file)
{
    char *buf = packet_buf;
    char *p = filename;
    char *options;
    char *data;
    struct open_file_t *open_file;
    static __lowmem struct s_PXENV_UDP_WRITE udp_write;
    static __lowmem struct s_PXENV_UDP_READ  udp_read;
    static __lowmem struct s_PXENV_FILE_OPEN file_open;
    static __lowmem struct s_PXENV_GET_FILE_SIZE get_file_size;
    const struct tftp_options *tftp_opt;
    int i = 0;
    int err;
    int buffersize;
    const uint8_t  *timeout_ptr;
    uint8_t  timeout;
    uint32_t oldtime;
    uint16_t tid;
    uint16_t opcode;
    uint16_t blk_num;
    uint32_t ip;
    uint32_t opdata, *opdata_ptr;
    
    open_file = allocate_socket();
    if (!open_file) {
	file->file_len = 0;
	file->open_file = NULL;
	return;
    }
    
    timeout_ptr = TimeoutTable;   /* Reset timeout */

 sendreq:
    udp_write.buffer.offs = OFFS_WRT(buf, 0);
    udp_write.buffer.seg  = 0;
    *(uint16_t *)buf = TFTP_RRQ;  /* TFTP opcode */
    buf += 2;
    
    ip = *(uint32_t *)p;      /* ip <- server override (if any) */
    p += 4;
    if (ip == 0) {
        /* Have prefix */
        strcpy(buf, path_prefix);
        buf += strlen(path_prefix);
        ip = server_ip;            /* Get the default server */
    }

    strcpy(buf, p);                /* Copy the filename */
    buf += strlen(p) + 1;          /* advance the pointer, null char included */
    
#if GPXE
    if (is_gpxe(packet_buf + 2)) {
        file_open.Status        = PXENV_STATUS_BAD_FUNC;
        file_open.FileName.offs = OFFS_WRT(packet_buf + 2, 0);
        file_open.FileName.seg  = 0;
        err = pxe_call(PXENV_FILE_OPEN, &file_open);
        if (err) 
            goto done;
        
        open_file->tftp_localport = -1;
        open_file->tftp_remoteport = file_open.FileHandle;
        get_file_size.FileHandle = file_open.FileHandle;
        
#if 0
        err = pxe_call(PXENV_GET_FILE_SIZE, &get_file_size);
        if (!err)
            open_file->tftp_filesize = get_file_size.FileSize;
        else
#endif
	    open_file->tftp_filesize = -1;
        goto done;
    }
#endif /* GPXE */
    
    open_file->tftp_remoteip = ip;
    tid = open_file->tftp_localport;   /* TID(local port No) */
    udp_write.ip        = ip;
    udp_write.gw        = ((udp_write.ip ^ MyIP) & net_mask) ? gate_way : 0;
    udp_write.src_port  = tid;
    udp_write.dst_port  = server_port;
    buf += fill_tail(buf);
    udp_write.buffer_size = buf - packet_buf;
    err = pxe_call(PXENV_UDP_WRITE, &udp_write);
    if (err || udp_write.status != 0)
        goto failure;            /* 
				  * In fact, the 'failure' target will not do
				  * a failure thing; it will move on to the
				  * next timeout, then tries again until
				  * _real_ time out
				  */

    /*
     * Danger, Will Robinson! We need to support tiemout
     * and retry lest we just lost a packet ...
     */
    
    /* Packet transmitted OK, now we need to receive */
    timeout = *timeout_ptr++;
    oldtime = jiffies();
    for (;;) {
        buf = packet_buf;
        udp_read.buffer.offs = OFFS_WRT(buf, 0);
        udp_read.buffer.seg = 0;
        udp_read.buffer_size = 2048;
        udp_read.dest_ip = MyIP;
        udp_read.d_port = tid;
        err = pxe_call(PXENV_UDP_READ, &udp_read);
        if (err) {
	    uint32_t now = jiffies();
	    if (now-oldtime >= timeout)
		goto failure;
	    continue;
        }

        /* Make sure the packet actually came from the server */
        if (udp_read.src_ip == open_file->tftp_remoteip)
            break;
    }
          
    /* Got packet; reset timeout */
    timeout_ptr = TimeoutTable;
    open_file->tftp_remoteport = udp_read.s_port;
    
    /* filesize <- -1 == unknown */
    open_file->tftp_filesize = -1;
    /* Default blksize unless blksize option negotiated */
    open_file->tftp_blksize = TFTP_BLOCKSIZE;
    buffersize = udp_read.buffer_size - 2;  /* bytes after opcode */
    if (buffersize < 0)
        goto failure;                     /* Garbled reply */
    
    /*
     * Get the opcode type, and parse it 
     */
    opcode = *(uint16_t *)packet_buf;
    switch (opcode) {
    case TFTP_ERROR:
        open_file->tftp_filesize = 0;
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
            goto failure;
        data = packet_buf + 2;
        blk_num = *(uint16_t *)data;
        data += 2;
        if (blk_num != htons(1))
            goto failure;
        open_file->tftp_lastpkt = blk_num;
        if (buffersize > TFTP_BLOCKSIZE)
            goto err_reply;  /* Corrupt */
        else if (buffersize < TFTP_BLOCKSIZE) {
            /* 
             * This is the final EOF packet, already...
             * We know the filesize, but we also want to 
             * ack the packet and set the EOF flag.
             */
            open_file->tftp_filesize = buffersize;
            open_file->tftp_goteof = 1;
            ack_packet(open_file, blk_num);
        }

        open_file->tftp_bytesleft = buffersize;
        open_file->tftp_dataptr = open_file->tftp_pktbuf;
        memcpy(MK_PTR(PKTBUF_SEG, open_file->tftp_pktbuf), data, buffersize);
	break;

    case TFTP_OACK:
        /* 
         * Now we need to parse the OACK packet to get the transfer
         * and packet sizes.
         */

        options = packet_buf + 2;
	p = options;
        
	while (buffersize) {
	    char *opt = p;

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
            opdata_ptr = (uint32_t *)((char *)open_file + tftp_opt->offset);
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
    if (!open_file->tftp_filesize) {
        free_socket(open_file);
	file->file_len  = 0;
	file->open_file = NULL;
	return;
    }
    file->open_file = (void *)open_file;
    file->file_len  = open_file->tftp_filesize;
    return;

err_reply:
    /* Build the TFTP error packet */
    tftp_error(open_file, TFTP_EOPTNEG, "TFTP protocol error");
    printf("TFTP server sent an incomprehesible reply\n");
    kaboom();
        
failure:
    timeout_ptr++;
    if (*timeout_ptr)
        goto sendreq;  /* Try again */
}
 

/*
 * Store standard filename prefix
 */
static void get_prefix(void)
{
    int len;
    char *p;
    char c;
    
    if (DHCPMagic & 0x04)         /* Did we get a path prefix option */
        goto got_prefix;
    
    strcpy(path_prefix, boot_file);
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
    
 got_prefix:
    printf("TFTP prefix: %s\n", path_prefix);
    strcpy(CurrentDirName, path_prefix);
}

 /*
  * try to load a config file, if found, return 1, or return 0
  *
  */   
static int try_load(char *config_name)
{
    com32sys_t regs;
    
    printf("Trying to load: %-50s ", config_name);
    pxe_mangle_name(KernelName, config_name);
    
    memset(&regs, 0, sizeof regs);
    regs.edi.w[0] = OFFS_WRT(KernelName, 0);
    call16(core_open, &regs, &regs);
    if (regs.eflags.l & EFLAGS_ZF) {
        printf("   [FAILED]\n");
        return 0;
    } else {
        printf("   [  OK  ]\n");
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
    char *p;   
    uint8_t *uuid_ptr;
    int tries = 8;
    
    get_prefix();
    if (DHCPMagic & 0x02) {
        /* We got a DHCP option, try it first */
	if (try_load(boot_file))
	    return 0;
    }
    
    /*
     * Have to guess config file name ...
     */  
    memcpy(ConfigName, cfgprefix, strlen(cfgprefix));
    config_file = ConfigName + strlen(cfgprefix);
      
    /* Try loading by UUID */
    if (have_uuid) {
        uuid_ptr  = uuid_dashes;
	p = config_file;
        while (*uuid_ptr) {
            int len = *uuid_ptr;
            char *src = uuid;
            
            lchexbytes(p, src, len);
            p += len * 2;
            src += len;
            uuid_ptr++;
            *p++ = '-';
        }
        /* Remove last dash and zero-terminate */
	*--p = '\0';
	if (try_load(ConfigName))
            return 0;
    }

    /* Try loading by MAC address */
    strcpy(config_file, MAC_str);
    if (try_load(ConfigName))
        return 0;
    
    /* Nope, try hexadecimal IP prefixes... */
    uchexbytes(config_file, (uint8_t *)&MyIP, 4);     /* Convet to hex string */
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

    printf("Unable to locate configuration file\n");
    kaboom();
}

/*
 * Generate the botif string, and the hardware-based config string 
 */
static void make_bootif_string(void)
{
    char mac[18];
    char *src = mac;
    char *dst = MAC_str;
    int i = MAC_len + 1;
    
    *(uint8_t *)src++ = MAC_type;
    memcpy(src, MAC, MAC_len);
    src = mac;
    for (; i > 0; i--) {
        lchexbytes(dst, src, 1);
        dst += 2;
        src += 1;
        *dst++ = '-';
    }
    *(dst - 1) = 0;   /* Drop the last '-' and null-terminate string */
    strcat(BOOTIFStr, "BOOTIF=");
    strcat(BOOTIFStr, MAC_str);

#if 0
    printf("%s\n", BOOTIFStr);
#endif
}
/*
 * Generate an ip=<client-ip>:<boot-server-ip>:<gw-ip>:<netmask>
 * option into IPOption based on a DHCP packet in trackbuf.
 *
 */
static void genipopt(void)
{
    char *p = IPOption;
    int ip_len;
    
    strcpy(p, "ip=");
    p += 3;
    
    ip_len = gendotquad(p, MyIP);
    p += ip_len;
    *p++ = ':';

    ip_len = gendotquad(p, server_ip);
    p += ip_len;
    *p++ = ':';

    ip_len = gendotquad(p, gate_way);
    p += ip_len;
    *p++ = ':';
    
    ip_len = gendotquad(p, net_mask);
}
    

/* Generate ip= option and print the ip adress */
static void ip_init(void)
{
    uint32_t ip = MyIP;
       
    genipopt();
    gendotquad(dot_quad_buf, ip);
    
    ip = ntohl(ip);
    printf("My IP address seems to be %08X %s\n", ip, dot_quad_buf);
    printf("%s\n", IPOption);
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
static int pxe_init(void)
{
    extern void pxe_int1a(void);
    char plan = 'A';
    uint16_t seg, off;
    uint16_t code_seg, code_len;
    uint16_t data_seg, data_len;
    char *base = GET_PTR(InitStack);
    com32sys_t regs;
    const char *type;
    const struct pxenv_t *pxenv;
    const struct pxe_t *pxe;

    /* Assume API version 2.1 */
    APIVer = 0x201;
    
    /* Plan A: !PXE structure as SS:[SP + 4] */
    off = *(uint16_t *)(base + 48);
    seg = *(uint16_t *)(base + 50);
    pxe = MK_PTR(seg, off);
    if (is_pxe(pxe)) 
        goto have_pxe;

    /* Plan B: PXENV+ structure at [ES:BX] */
    plan++;
    off = *(uint16_t *)(base + 24);  /* Original BX */
    seg = *(uint16_t *)(base + 4);   /* Original ES */
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
    printf("No !PXE or PXENV+ API found; we're dead...\n");
    kaboom();
    
 have_pxenv:
    APIVer = pxenv->version;
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
    printf("%s entry point found (we hope) at %04X:%04X via plan %c\n",
	   type, PXEEntry.seg, PXEEntry.offs, plan);
    printf("UNDI code segment at %04X len %04X\n", code_seg, code_len);
    printf("UNDI data segment at %04X len %04X\n", data_seg, data_len);

    code_seg = code_seg + ((code_len + 15) >> 4);
    data_seg = data_seg + ((data_len + 15) >> 4);

    real_base_mem = max(code_seg,data_seg) >> 6; /* Convert to kilobytes */

    return 0;
}                                  

/*
 * Initialize UDP stack 
 *
 */
static void udp_init(void)
{
    int err;
    static __lowmem struct s_PXENV_UDP_OPEN udp_open;
    udp_open.src_ip = MyIP;
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
    ip_init();

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
    
    /* Initialize the Files structure */
    files_init();

    /* do the pxe initialize */
    pxe_init();

    /* Network-specific initialization */
    network_init();

    /* Initialize network-card-specific idle handling */
    pxe_idle_init();

    return 0;
}

inline void reset_pxe(void)
{
    static __lowmem struct s_PXENV_UDP_CLOSE udp_close;
    pxe_call(PXENV_UDP_CLOSE, &udp_close);
}

/*
 * This function unloads the PXE and UNDI stacks and 
 * unclaims the memory.
 */
void unload_pxe(void)
{
    uint8_t api;
    const uint8_t *api_ptr;
    uint16_t flag = 0;
    int err;
    int int_addr;
    static __lowmem struct s_PXENV_UNLOAD_STACK unload_stack;

    pxe_idle_cleanup();
   
    if (KeepPXE) {
	/*
	 * We want to keep PXE around, but still we should reset
	 * it to the standard bootup configuration.
	 */
	reset_pxe();
	return;
    }

    api_ptr = major_ver(APIVer) >= 2 ? new_api_unload : old_api_unload;
    while((api = *api_ptr++)) {
	memset(&unload_stack, 0, sizeof unload_stack);
	err = pxe_call(api, &unload_stack);
	if (err || unload_stack.Status != PXENV_STATUS_SUCCESS)
	    goto cant_free;
    }
    
    flag = 0xff00;
    if (real_base_mem <= BIOS_fbm)  /* Santiy check */
	goto cant_free;
    flag ++;
    
    /* Check that PXE actually unhooked the INT 0x1A chain */
    int_addr = (int)MK_PTR(*(uint16_t *)(4*0x1a+2), *(uint16_t *)(4*0x1a));
    int_addr >>= 10;
    if (int_addr >= real_base_mem || int_addr < BIOS_fbm) {
	BIOS_fbm = real_base_mem;
	return;
    }
    
cant_free:
    printf("Failed to free base memory error %04x-%08x\n", 
	   flag, *(uint32_t *)(4 * 0x1a));
    return;
}
    

    
const struct fs_ops pxe_fs_ops = {
    .fs_name       = "pxe",
    .fs_flags      = FS_NODEV,
    .fs_init       = pxe_fs_init,
    .searchdir     = pxe_searchdir,
    .getfssec      = pxe_getfssec,
    .close_file    = pxe_close_file,
    .mangle_name   = pxe_mangle_name,
    .unmangle_name = pxe_unmangle_name,
    .load_config   = pxe_load_config,
    .iget_current  = NULL
};
