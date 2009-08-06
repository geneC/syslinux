#include <stdio.h>
#include <string.h>
#include <core.h>
#include <pxe.h>
#include <sys/cpu.h>

void subnet_mask(void *data, int opt_len)
{
    if (opt_len != 4)
	return;
    Netmask = *(uint32_t *)data;
}

void router(void *data, int opt_len)
{
    if (opt_len != 4)
	return;
    Gateway = *(uint32_t *)data;
}

void dns_servers(void *data, int opt_len)
{
    int num = opt_len >> 2;
    int i;

    if (num > DNS_MAX_SERVERS)
        num = DNS_MAX_SERVERS;

    for (i = 0; i < num; i++) {
        DNSServers[i] = *(uint32_t *)data;
        data += 4;
    }
    
    /* NOT SURE FOR NOW */
    LastDNSServer = OFFS_WRT(&DNSServers[num - 1], 0);
}

void local_domain(void *data, int opt_len)
{
    com32sys_t regs;
    char *p = (char *)data + opt_len;
    char end = *p;

    memset(&regs, 0, sizeof regs);
    *p = '\0';   /* Zero-terminate option */
    regs.esi.w[0] = OFFS_WRT(data, 0);
    regs.edi.w[0] = OFFS_WRT(LocalDomain, 0);
    call16(dns_mangle, &regs, NULL);
    *p = end;    /* Resotre ending byte */
}

void vendor_encaps(void *data, int opt_len)
{
    /* Only recongnize PXELINUX options */
    parse_dhcp_options(data, opt_len, 208);
}

void option_overload(void *data, int opt_len)
{
    if (opt_len != 1)
	return;
    OverLoad = *(uint8_t *)data;
}


void server(void *data, int opt_len)
{
    uint32_t ip;

    if (opt_len != 4)
	return;
    
    if (ServerIP)
        return;
    
    ip = *(uint32_t *)data;
    if (ip_ok(ip))
        ServerIP = ip;
}

void client_identifier(void *data, int opt_len)
{
    if (opt_len > MAC_MAX || opt_len < 2 ||
        MACLen != (opt_len >> 8) || 
        *(uint8_t *)data != MACType)
        return;

    opt_len --;
    MACLen = opt_len & 0xff;
    memcpy(MAC, data+1, opt_len);
    MAC[opt_len] = 0;
}

void bootfile_name(void *data, int opt_len)
{
    strncpy(BootFile, data, opt_len);
    BootFile[opt_len] = 0;
}
   
void uuid_client_identifier(void *data, int opt_len)
{
    int type = *(uint8_t *)data;
    if (opt_len != 17 ||
        (type | HaveUUID))
        return;

    HaveUUID = 1;
    UUIDType = type;
    memcpy(UUID, data+1, 16);
    UUID[16] = 0;
}

void pxelinux_configfile(void *data, int opt_len)
{
    DHCPMagic |= 2;
    strncpy(ConfigName, data, opt_len);
    ConfigName[opt_len] = 0;
}

void pxelinux_pathprefix(void *data,int opt_len)
{
    DHCPMagic |= 4;
    strncpy(PathPrefix, data, opt_len);
    PathPrefix[opt_len] = 0;
}

void pxelinux_reboottime(void *data, int opt_len)
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

struct dhcp_options dhcp_opts[] = { 
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
void parse_dhcp_options(void *option, int size, int filter)
{
    uint8_t opt_num;
    uint8_t opt_len;
    uint8_t opt_filter = filter == 208 ? 208 : 0;
    int opt_entries = sizeof(dhcp_opts) / sizeof(dhcp_opts[0]);
    int i = 0;
    char *p = option;
    struct dhcp_options *opt;
    
    if (opt_filter)
        printf("***NOTE!:*** we hit a pxelinux-specific options\n");
    
    while (size --) {
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
 *
 ;
 ; parse_dhcp
 ;
 ; Parse a DHCP packet.  This includes dealing with "overloaded"
 ; option fields (see RFC 2132, section 9.3)
 ;
 ; This should fill in the following global variables, if the
 ; information is present:
 ;
 ; MyIP		- client IP address
 ; ServerIP	- boot server IP address
 ; Netmask	- network mask
 ; Gateway	- default gateway router IP
 ; BootFile	- boot file name
 ; DNSServers	- DNS server IPs
 ; LocalDomain	- Local domain name
 ; MACLen, MAC	- Client identifier, if MACLen == 0
 ;
 ; This assumes the DHCP packet is in "trackbuf".
 ;
*/
void parse_dhcp(int pkt_len)
{
    struct bootp_t *dhcp = (struct bootp_t *)trackbuf;
    int opt_len;

    OverLoad = 0;
    if (ip_ok(dhcp->yip))
        MyIP = dhcp->yip;
    
    if (ip_ok(dhcp->sip))
        ServerIP = dhcp->sip;
    
    opt_len = (char *)dhcp + pkt_len - (char *)&dhcp->options;
    if (opt_len && (dhcp->option_magic == BOOTP_OPTION_MAGIC)) 
        parse_dhcp_options(&dhcp->options, opt_len, 0);

    if (OverLoad & 1) 
        parse_dhcp_options(&dhcp->bootfile, 128, 0);
    else if (dhcp->bootfile[0]) 
            strcpy(BootFile, dhcp->bootfile);
    
    if (OverLoad & 2) 
        parse_dhcp_options(dhcp->sname, 64, 0);
}  
