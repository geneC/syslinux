#include <stdio.h>
#include <string.h>

#include "core.h"


void load_config(com32sys_t *regs)
{
    char *config_name = "extlinux.conf";
    com32sys_t out_regs;
    
    strcpy(ConfigName, config_name);
    *(uint32_t *)CurrentDirName = 0x00002f2e;  

    regs->edi.w[0] = ConfigName;
    memset(&out_regs, 0, sizeof out_regs);
    call16(core_open, regs, &out_regs);

    regs->eax.w[0] = out_regs.eax.w[0];

#if 0
    printf("the zero flag is %s\n", regs->eax.w[0] ?          \
           "CLEAR, means we found the config file" :
           "SET, menas we didn't find the config file");
#endif
}
