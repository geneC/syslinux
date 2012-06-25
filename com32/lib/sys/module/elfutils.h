#if __SIZEOF_POINTER__ == 4
#include <i386/elfutils.h>
#elif __SIZEOF_POINTER__ == 8
#include <x86_64/elfutils.h>
#else
#error "unsupported architecture"
#endif
