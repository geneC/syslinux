#include <sys/gpxe.h>
#include <syslinux/config.h>
#include <string.h>

bool is_gpxe(void)
{
    const struct syslinux_version *sv;
    com32sys_t reg;
    struct s_PXENV_FILE_CHECK_API *fca;

    sv = syslinux_version();
    if (sv->filesystem != SYSLINUX_FS_PXELINUX)
        return false;           /* Not PXELINUX */

    fca = __com32.cs_bounce;
    memset(fca, 0, sizeof *fca);
    fca->Size = sizeof *fca;
    fca->Magic = 0x91d447b2;

    memset(&reg, 0, sizeof reg);
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = 0x00e6;      /* PXENV_FILE_API_CHECK */
    reg.edi.w[0] = OFFS(fca);
    reg.es = SEG(fca);

    __intcall(0x22, &reg, &reg);

    if (reg.eflags.l & EFLAGS_CF)
        return false;           /* Cannot invoke PXE stack */

    if (reg.eax.w[0] || fca->Status)
        return false;           /* PXE failure */

    if (fca->Magic != 0xe9c17b20)
        return false;           /* Incorrect magic */

    if (fca->Size < sizeof *fca)
        return false;           /* Short return */

    if (!(fca->APIMask & (1 << 5)))
        return false;           /* No FILE EXEC */

    return true;
}

