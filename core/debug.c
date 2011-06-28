#include "core.h"
#include <dprintf.h>

void pm_debug_msg(com32sys_t *regs)
{
    dprintf("%s\n", MK_PTR(0, regs->eax.w[0]));
}
