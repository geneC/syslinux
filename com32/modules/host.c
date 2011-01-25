#include <stdio.h>
#include <console.h>
#include <string.h>
#include <netinet/in.h>
#include <com32.h>

static struct in_addr dnsresolve(const char *hostname)
{
    com32sys_t regs;
    struct in_addr addr;

    strcpy(__com32.cs_bounce, hostname);
    
    regs.eax.w[0] = 0x0010;
    regs.es       = SEG(__com32.cs_bounce);
    regs.ebx.w[0] = OFFS(__com32.cs_bounce);
    __intcall(0x22, &regs, &regs);

    addr.s_addr = regs.eax.l;
    return addr;
}

int main(int argc, char *argv[])
{
    int i;
    struct in_addr addr;

    openconsole(&dev_null_r, &dev_stdcon_w);

    for (i = 1; i < argc; i++) {
	addr = dnsresolve(argv[i]);

	printf("%-39s %08X %d.%d.%d.%d\n",
	       argv[i], ntohl(addr.s_addr),
	       ((uint8_t *)&addr.s_addr)[0], 
	       ((uint8_t *)&addr.s_addr)[1], 
	       ((uint8_t *)&addr.s_addr)[2], 
	       ((uint8_t *)&addr.s_addr)[3]);
    }

    return 0;
}
