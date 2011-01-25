#include <sys/gpxe.h>
#include <syslinux/config.h>
#include <string.h>

bool is_gpxe(void)
{
    const struct syslinux_version *sv;
    com32sys_t reg;
    struct s_PXENV_FILE_CHECK_API *fca;
    bool gpxe;

    sv = syslinux_version();
    if (sv->filesystem != SYSLINUX_FS_PXELINUX)
        return false;           /* Not PXELINUX */

    fca = lzalloc(sizeof *fca);
    if (!fca)
	return false;
    fca->Size = sizeof *fca;
    fca->Magic = 0x91d447b2;

    memset(&reg, 0, sizeof reg);
    reg.eax.w[0] = 0x0009;
    reg.ebx.w[0] = 0x00e6;      /* PXENV_FILE_API_CHECK */
    /* reg.edi.w[0] = OFFS(fca); */
    reg.es = SEG(fca);

    __intcall(0x22, &reg, &reg);

    gpxe = true;

    if (reg.eflags.l & EFLAGS_CF)
	gpxe = false;           /* Cannot invoke PXE stack */

    if (reg.eax.w[0] || fca->Status)
        gpxe = false;           /* PXE failure */

    if (fca->Magic != 0xe9c17b20)
        gpxe = false;           /* Incorrect magic */

    if (fca->Size < sizeof *fca)
        gpxe = false;           /* Short return */

    /* XXX: The APIs to test for should be a passed-in option */
    if (!(fca->APIMask & (1 << 5)))
	gpxe = false;           /* No FILE EXEC */

    lfree(fca);
    return gpxe;
}
