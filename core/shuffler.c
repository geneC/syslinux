#include "core.h"

#ifdef __FIRMWARE_BIOS__

extern const char __bcopyxx_len[]; /* Linker script absolute symbol */
const size_t __syslinux_shuffler_size = (size_t)__bcopyxx_len;

#endif /* __FIRMWARE_BIOS__ */
