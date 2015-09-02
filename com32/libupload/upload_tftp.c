/*
 * TFTP data output backend
 */

#include <string.h>
#include <stdio.h>
#include <syslinux/pxe.h>
#include <syslinux/config.h>
#include <netinet/in.h>
#include <sys/times.h>
#include <fs/pxe/pxe.h>
#include <fs/pxe/url.h>
#include "upload_backend.h"

const char *tftp_string_error_message[]={
"Unknown error",
"File not found",
"Access Denied",
"Disk Full",
"Illegal Operation",
"Unknown Transfert ID",
"File already exists",
"Unknown User",
"Negociation failed",
"Unable to resolve hostname", // not in RFC
"Unable to connect", // not in RFC
"No Error",
};

static int upload_tftp_write(struct upload_backend *be) {
    const union syslinux_derivative_info *sdi =
	syslinux_derivative_info();
    struct url_info url;
    struct inode inode;
    char url_path[255] = {0};
    uint32_t ip;
    int err;

    if (be->argv[1]) {
        ip = pxe_dns(be->argv[1]);
        if (!ip) {
            dprintf("\nUnable to resolve hostname: %s\n", be->argv[1]);
            return -TFTP_ERR_UNABLE_TO_RESOLVE;
        }
    } else {
        ip   = sdi->pxe.ipinfo->serverip;
        if (!ip) {
            dprintf("\nNo server IP address\n");
            return -TFTP_ERR_UNABLE_TO_CONNECT;
        }
    }

    snprintf(url_path, sizeof(url_path), "tftp://%u.%u.%u.%u/%s",
	((uint8_t *)&ip)[0],
	((uint8_t *)&ip)[1],
	((uint8_t *)&ip)[2],
	((uint8_t *)&ip)[3],
	be->argv[0]);

    parse_url(&url, url_path);
    url.ip = ip;

    dprintf("Connecting to %s to send %s\n", url.host, url.path);
    err = tftp_put(&url, 0, &inode, NULL, be->outbuf, be->zbytes);

    if (-err != TFTP_OK)
	printf("upload_tftp_write: TFTP server returned error %d : %s\n", err, tftp_string_error_message[-err]);

    return -err;
}

struct upload_backend upload_tftp = {
    .name       = "tftp",
    .helpmsg    = "filename [tftp_server]",
    .minargs    = 1,
    .write      = upload_tftp_write,
};
