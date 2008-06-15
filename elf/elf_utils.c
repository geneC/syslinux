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

unsigned long elf_gnu_hash(const unsigned char *name) {
	unsigned long h = 5381;
	unsigned char c;
	
	for (c = *name; c != '\0'; c = *++name) {
		h = h * 33 + c;
	}
	
	return h & 0xFFFFFFFF;
}
