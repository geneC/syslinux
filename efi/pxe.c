/*
 * Copyright 2013-2014 Intel Corporation - All Rights Reserved
 */

#include <syslinux/firmware.h>
#include <syslinux/pxe_api.h>
#include "efi.h"
#include "net.h"
#include "fs/pxe/pxe.h"

const struct url_scheme url_schemes[] = {
    { "tftp", tftp_open, 0 },
    { "http", http_open, O_DIRECTORY },
    { "ftp",  ftp_open,  O_DIRECTORY },
    { NULL, NULL, 0 },
};

/**
 * Network stack-specific initialization
 */
void net_core_init(void)
{
    http_bake_cookies();
}

void pxe_init_isr(void) {}
void gpxe_init(void) {}
void pxe_idle_init(void) {}

int reset_pxe(void)
{
    return 0;
}

#define DNS_MAX_SERVERS 4		/* Max no of DNS servers */
uint32_t dns_server[DNS_MAX_SERVERS] = {0, };

__export uint32_t dns_resolv(const char *name)
{
    /*
     * Return failure on an empty input... this can happen during
     * some types of URL parsing, and this is the easiest place to
     * check for it.
     */
    if (!name || !*name)
	return 0;

    return 0;
}

int pxe_init(bool quiet)
{
    EFI_HANDLE *handles;
    EFI_STATUS status;
    UINTN nr_handles;

    status = LibLocateHandle(ByProtocol, &PxeBaseCodeProtocol,
			     NULL, &nr_handles, &handles);
    if (status != EFI_SUCCESS) {
	if (!quiet)
	    Print(L"No PXE Base Code Protocol\n");
	return -1;
    }

    return 0;
}

void net_parse_dhcp(void)
{
    EFI_PXE_BASE_CODE_MODE *mode;
    EFI_PXE_BASE_CODE *bc;
    unsigned int pkt_len = sizeof(EFI_PXE_BASE_CODE_PACKET);
    EFI_STATUS status;
    EFI_HANDLE *handles = NULL;
    UINTN nr_handles = 0;
    uint8_t hardlen;
    uint32_t ip;
    char dst[256];

    status = LibLocateHandle(ByProtocol, &PxeBaseCodeProtocol,
			 NULL, &nr_handles, &handles);
    if (status != EFI_SUCCESS)
	return;

    /* Probably want to use IPv4 protocol to decide which handle to use */
    status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[0],
			   &PxeBaseCodeProtocol, (void **)&bc);
    if (status != EFI_SUCCESS) {
	Print(L"Failed to lookup PxeBaseCodeProtocol\n");
    }

    mode = bc->Mode;

    /*
     * Get the DHCP client identifiers (query info 1)
     */
    Print(L"Getting cached packet ");
    parse_dhcp(&mode->DhcpDiscover.Dhcpv4, pkt_len);
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
    parse_dhcp(&mode->DhcpAck.Dhcpv4, pkt_len);
    /*
     * Save away MAC address (assume this is in query info 2. If this
     * turns out to be problematic it might be better getting it from
     * the query info 1 packet
     */
    hardlen = mode->DhcpAck.Dhcpv4.BootpHwAddrLen;
    MAC_len = hardlen > 16 ? 0 : hardlen;
    MAC_type = mode->DhcpAck.Dhcpv4.BootpHwType;
    memcpy(MAC, mode->DhcpAck.Dhcpv4.BootpHwAddr, MAC_len);

    /*
     * Get the boot file and other info. This lives in the CACHED_REPLY
     * packet (query info 3)
     */
    parse_dhcp(&mode->PxeReply.Dhcpv4, pkt_len);
    Print(L"\n");

    ip = IPInfo.myip;
    sprintf(dst, "%u.%u.%u.%u",
        ((const uint8_t *)&ip)[0],
        ((const uint8_t *)&ip)[1],
        ((const uint8_t *)&ip)[2],
        ((const uint8_t *)&ip)[3]);

    Print(L"My IP is %a\n", dst);
}
