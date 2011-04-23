#include <ctype.h>
#include <lwip/api.h>
#include "pxe.h"
#include "../../../version.h"
#include "url.h"

#define HTTP_PORT	80

static void http_close_file(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    if (socket->buf) {
	netbuf_delete(socket->buf);
        socket->buf = NULL;
    }
    if (socket->conn) {
	netconn_delete(socket->conn);
	socket->conn = NULL;
    }
}

static void http_fill_buffer(struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    void *data;
    u16_t len;
    err_t err;

    /* Clean up or advance an inuse netbuf */
    if (socket->buf) {
	if (netbuf_next(socket->buf) < 0) {
	    netbuf_delete(socket->buf);
	    socket->buf = NULL;
	}
    }
    /* If needed get a new netbuf */
    if (!socket->buf) {
	socket->buf = netconn_recv(socket->conn);
	if (!socket->buf) {
	    socket->tftp_goteof = 1;
	    if (inode->size == -1)
		inode->size = socket->tftp_filepos;
	    http_close_file(inode);
	    return;
	}
    }
    /* Report the current fragment of the netbuf */
    err = netbuf_data(socket->buf, &data, &len);
    if (err) {
	printf("netbuf_data err: %d\n", err);
	kaboom();
    }
    socket->tftp_dataptr = data;
    socket->tftp_filepos += len;
    socket->tftp_bytesleft = len;
    return;
}

static bool is_tspecial(int ch)
{
    bool tspecial = false;
    switch(ch) {
    case '(':  case ')':  case '<':  case '>':  case '@':
    case ',':  case ';':  case ':':  case '\\': case '"':
    case '/':  case '[':  case ']':  case '?':  case '=':
    case '{':  case '}':  case ' ':  case '\t':
	tspecial = true;
    	break;
    }
    return tspecial;
}

static bool is_ctl(int ch)
{
    return ch < 0x20;
}

static bool is_token(int ch)
{
    /* Can by antying except a ctl character or a tspecial */
    return !is_ctl(ch) && !is_tspecial(ch);
}

static bool append_ch(char *str, size_t size, size_t *pos, int ch)
{
    bool success = true;
    if ((*pos + 1) >= size) {
	*pos = 0;
	success = false;    
    } else {
	str[*pos] = ch;
	str[*pos + 1] = '\0';
	*pos += 1;
    }
    return success;
}

void http_open(struct url_info *url, struct inode *inode)
{
    struct pxe_pvt_inode *socket = PVT(inode);
    char header_buf[4096];
    int header_len;
    const char *next;
    char field_name[20];
    char field_value[1024];
    size_t field_name_len, field_value_len;
    err_t err;
    enum state {
	st_httpver,
	st_stcode,
	st_skipline,
	st_fieldfirst,
	st_fieldname,
	st_fieldvalue,
	st_skip_fieldname,
	st_skip_fieldvalue,
	st_eoh,
    } state;
    struct ip_addr addr;
    char location[1024], new_url[1024];
    uint32_t content_length; /* same as inode->size */
    size_t response_size;
    int status;
    int pos;
    int redirect_count;

    socket->fill_buffer = http_fill_buffer;
    socket->close = http_close_file;
    redirect_count = 0;

restart:
    /* Reset all of the variables */
    inode->size = content_length = -1;

    /* Start the http connection */
    socket->conn = netconn_new(NETCONN_TCP);
    if (!socket->conn) {
	printf("netconn_new failed\n");
        return;
    }

    addr.addr = url->ip;
    if (!url->port)
	url->port = HTTP_PORT;
    err = netconn_connect(socket->conn, &addr, url->port);
    if (err) {
	printf("netconn_connect error %d\n", err);
	goto fail;
    }

    strcpy(header_buf, "GET /");
    header_len = 5;
    header_len += url_escape_unsafe(header_buf+5, url->path,
				    sizeof header_buf - 5);
    if (header_len > sizeof header_buf)
	goto fail;		/* Buffer overflow */
    header_len += snprintf(header_buf + header_len,
			   sizeof header_buf - header_len,
			   " HTTP/1.0\r\n"
			   "Host: %s\r\n"
			   "User-Agent: PXELINUX/%s\r\n"
			   "Connection: close\r\n"
			   "\r\n",
			   url->host, VERSION_STR);
    if (header_len > sizeof header_buf)
	goto fail;		/* Buffer overflow */

    err = netconn_write(socket->conn, header_buf, header_len, NETCONN_NOCOPY);
    if (err) {
	printf("netconn_write error %d\n", err);
	goto fail;
    }

    /* Parse the HTTP header */
    state = st_httpver;
    pos = 0;
    status = 0;
    response_size = 0;
    field_value_len = 0;
    field_name_len = 0;

    while (state != st_eoh) {
	int ch = pxe_getc(inode);
	/* Eof before I finish paring the header */
	if (ch == -1)
	    goto fail;
#if 0
        printf("%c", ch);
#endif
	response_size++;
	if (ch == '\r' || ch == '\0')
	    continue;
	switch (state) {
	case st_httpver:
	    if (ch == ' ') {
		state = st_stcode;
		pos = 0;
	    }
	    break;

	case st_stcode:
	    if (ch < '0' || ch > '9')
	       goto fail;
	    status = (status*10) + (ch - '0');
	    if (++pos == 3)
		state = st_skipline;
	    break;

	case st_skipline:
	    if (ch == '\n')
		state = st_fieldfirst;
	    break;

	case st_fieldfirst:
	    if (ch == '\n')
		state = st_eoh;
	    else if (isspace(ch)) {
		/* A continuation line */
		state = st_fieldvalue;
		goto fieldvalue;
	    }
	    else if (is_token(ch)) {
		/* Process the previous field before starting on the next one */
		if (strcasecmp(field_name, "Content-Length") == 0) {
		    next = field_value;
		    /* Skip leading whitespace */
		    while (isspace(*next))
			next++;
		    content_length = 0;
		    for (;(*next >= '0' && *next <= '9'); next++) {
			if ((content_length * 10) < content_length)
			    break;
			content_length = (content_length * 10) + (*next - '0');
		    }
		    /* In the case of overflow or other error ignore
		     * Content-Length.
		     */
		    if (*next)
			content_length = -1;
		}
		else if (strcasecmp(field_name, "Location") == 0) {
		    next = field_value;
		    /* Skip leading whitespace */
		    while (isspace(*next))
			next++;
		    strcpy(location, next);
		}
		/* Start the field name and field value afress */
		field_name_len = 1;
		field_name[0] = ch;
		field_name[1] = '\0';
		field_value_len = 0;
		field_value[0] = '\0';
		state = st_fieldname;
	    }
	    else /* Bogus try to recover */
		state = st_skipline;
	    break;

	case st_fieldname:
	    if (ch == ':' ) {
		state = st_fieldvalue;
	    }
	    else if (is_token(ch)) {
		if (!append_ch(field_name, sizeof field_name, &field_name_len, ch))
		    state = st_skip_fieldname;
	    }
	    /* Bogus cases try to recover */
	    else if (ch == '\n')
		state = st_fieldfirst;
	    else 
		state = st_skipline;
	    break;

	 case st_fieldvalue:
	    if (ch == '\n')
		state = st_fieldfirst;
	    else {
	    fieldvalue:
		if (!append_ch(field_value, sizeof field_value, &field_value_len, ch))
		    state = st_skip_fieldvalue;
	    }
	    break;

	/* For valid fields whose names are longer than I choose to support. */
	case st_skip_fieldname:
	    if (ch == ':')
		state = st_skip_fieldvalue;
	    else if (is_token(ch))
		state = st_skip_fieldname;
	    /* Bogus cases try to recover */
	    else if (ch == '\n')
		state = st_fieldfirst;
	    else
		state = st_skipline;
	    break;

	/* For valid fields whose bodies are longer than I choose to support. */
	case st_skip_fieldvalue:
	    if (ch == '\n')
		state = st_fieldfirst;
	    break;
	
	case st_eoh:
	   break; /* Should never happen */
	}
    }

    if (state != st_eoh)
	status = 0;

    switch (status) {
    case 200:
	/*
	 * All OK, need to mark header data consumed and set up a file
	 * structure...
	 */
	/* Treat the remainder of the bytes as data */
	socket->tftp_filepos -= response_size;
	break;
    case 301:
    case 302:
    case 303:
    case 307:
	/* A redirect */
	if (!location[0])
	    goto fail;
	redirect_count++;
	if (redirect_count > 5)
	    goto fail;
	strlcpy(new_url, location, sizeof new_url);
	parse_url(url, new_url);
	url_set_ip(url);
	http_close_file(inode);
	/* XXX: This needs to go all the way back to scheme selection */
	goto restart;
	break;
    default:
	goto fail;
	break;
    }
    return;
fail:
    inode->size = 0;
    http_close_file(inode);
    return;
}
