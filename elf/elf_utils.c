#include "elf_utils.h"

unsigned long elf_hash(const unsigned char *name) {
	unsigned long h = 0;
	unsigned long g;
	
	while (*name) {
		h = (h << 4) + *name++;
		if ((g = h & 0xF0000000))
			h ^= g >> 24;
		
		h &= ~g;
	}
	
	return h;
}
