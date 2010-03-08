#include <stdio.h>
#include <string.h>
#include <core.h>
#include <sys/cpu.h>
#include "pxe.h"

char LocalDomain[256];

int over_load;
uint8_t uuid_type;
char uuid[17];

static void parse_dhcp_options(void *, int, uint8_t);

static void subnet_mask(void *data, int opt_len)
{
    if (opt_len != 4)
	return;
    net_mask = *(uint32_t *)data;
}

static void router(void *data, int opt_len)
{
    if (opt_len != 4)
	return;
    gate_way = *(uint32_t *)data;
}

static void dns_servers(void *data, int opt_len)
{
    int num = opt_len >> 2;
    int i;

    if (num > DNS_MAX_SERVERS)
        num = DNS_MAX_SERVERS;

    for (i = 0; i < num; i++) {
        dns_server[i] = *(uint32_t *)data;
        data += 4;
    }

#if 0
    /*
     * if you find you got no corret DNS server, you can add
     * it here manually. BUT be carefull the DNS_MAX_SERVERS
     */
    if (i < DNS_MAX_SERVERS ) {
        dns_server[i++] = your_master_dns_server;
        dns_server[i++] = your_second_dns_server;
    }
#endif
}

static void local_domain(void *data, int opt_len)
{
    char *p = (char *)data + opt_len;
    char *ld = LocalDomain;
    char end = *p;
    
    *p = '\0';   /* Zero-terminate option */
    dns_mangle(&ld, data);
    *p = end;    /* Restore ending byte */
}

static void vendor_encaps(void *data, int opt_len)
{
    /* Only recongnize PXELINUX options */
    parse_dhcp_options(data, opt_len, 208);
}

static void option_overload(void *data, int opt_len)
{
    if (opt_len != 1)
	return;
    over_load = *(uint8_t *)data;
}


static void server(void *data, int opt_len)
{
    uint32_t ip;

    if (opt_len != 4)
	return;
    
    if (server_ip)
        return;
    
    ip = *(uint32_t *)data;
    if (ip_ok(ip))
        server_ip = ip;
}

static void client_identifier(void *data, int opt_len)
{
    if (opt_len > MAC_MAX || opt_len < 2 ||
        MAC_len != (opt_len >> 8) || 
        *(uint8_t *)data != MAC_type)
        return;

    opt_len --;
    MAC_len = opt_len & 0xff;
    memcpy(MAC, data+1, opt_len);
    MAC[opt_len] = 0;
}

static void bootfile_name(void *data, int opt_len)
{
    strncpy(boot_file, data, opt_len);
    boot_file[opt_len] = 0;
}
   
static void uuid_client_identifier(void *data, int opt_len)
{
    int type = *(uint8_t *)data;
    if (opt_len != 17 || type != 0 || have_uuid)
        return;

    have_uuid = true;
    uuid_type = type;
    memcpy(uuid, data+1, 16);
    uuid[16] = 0;
}

static void pxelinux_configfile(void *data, int opt_len)
{
    DHCPMagic |= 2;
    strncpy(ConfigName, data, opt_len);
    ConfigName[opt_len] = 0;
}

static void pxelinux_pathprefix(void *data, int opt_len)
{
    DHCPMagic |= 4;
    strncpy(path_prefix, data, opt_len);
    path_prefix[opt_len] = 0;
}

static void pxelinux_reboottime(void *data, int opt_len)
{
    if ((opt_len && 0xff) != 4)
        return ;
    
    RebootTime = ntohl(*(uint32_t *)data);
    DHCPMagic |= 8;     /* Got reboot time */
}


struct dhcp_options {
    int opt_num;
    void (*fun) (void *, int);
};

static struct dhcp_options dhcp_opts[] = { 
    {1,   subnet_mask},
    {3,   router},
    {6,   dns_servers},
    {15,  local_domain},
    {43,  vendor_encaps},
    {52,  option_overload},
    {54,  server},
    {61,  client_identifier},
    {67,  bootfile_name},
    {97,  uuid_client_identifier},
    {209, pxelinux_configfile},
    {210, pxelinux_pathprefix},
    {211, pxelinux_reboottime}
};

/*
 * Parse a sequence of DHCP options, pointed to by _option_; 
 * -- some DHCP servers leave option fields unterminated
 * in violation of the spec.
 *
 * filter  contains the minimum value for the option to recognize
 * -- this is used to restrict parsing to PXELINUX-specific options only.
 */  
static void parse_dhcp_options(void *option, int size, uint8_t opt_filter)
{
    uint8_t opt_num;
    uint8_t opt_len;
    int opt_entries = sizeof(dhcp_opts) / sizeof(dhcp_opts[0]);
    int i = 0;
    char *p = option;
    struct dhcp_options *opt;
    
    while (size--) {
        opt_num = *p++;

        if (!size)
            break;
        if (opt_num == 0)
            continue;
        if (opt_num == 0xff)
            break;
        
        /* Anything else will have a lenght filed */
        opt_len = *p++; /* c  <- option lenght */
        size = size - opt_len - 1;
        if (size < 0)
            break;
        if (opt_num < opt_filter) {       /* Is the option value valid */
            option += opt_len;   /* Try next */
            continue;
        }

        opt = dhcp_opts;
        for (i = 0; i < opt_entries; i++) {
            if (opt_num == opt->opt_num) {
                opt->fun(p, opt_len);
                break;
            }            
            opt ++;
        }
        
        /* parse next */
        p += opt_len;
    }
}
 
/*
 * parse_dhcp
 *
 * Parse a DHCP packet.  This includes dealing with "overloaded"
 * option fields (see RFC 2132, section 9.3)
 *
 * This should fill in the following global variables, if the
 * information is present:
 *
 * MyIP		- client IP address
 * server_ip	- boot server IP address
 * net_mask	- network mask
 * gate_way	- default gateway router IP
 * boot_file	- boot file name
 * DNSServers	- DNS server IPs
 * LocalDomain	- Local domain name
 * MAC_len, MAC	- Client identifier, if MAC_len == 0
 *
 * This assumes the DHCP packet is in "trackbuf".
 *
 */
void parse_dhcp(int pkt_len)
{
    struct bootp_t *dhcp = (struct bootp_t *)trackbuf;
    int opt_len;

    over_load = 0;
    if (ip_ok(dhcp->yip))
        MyIP = dhcp->yip;
    
    if (ip_ok(dhcp->sip))
        server_ip = dhcp->sip;
    
    opt_len = (char *)dhcp + pkt_len - (char *)&dhcp->options;
    if (opt_len && (dhcp->option_magic == BOOTP_OPTION_MAGIC)) 
        parse_dhcp_options(&dhcp->options, opt_len, 0);

    if (over_load & 1) 
        parse_dhcp_options(&dhcp->bootfile, 128, 0);
    else if (dhcp->bootfile[0]) 
            strcpy(boot_file, dhcp->bootfile);
    
    if (over_load & 2) 
        parse_dhcp_options(dhcp->sname, 64, 0);
}  
