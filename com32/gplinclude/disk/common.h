#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>

#define SECTOR 512		/* bytes/sector */

#undef PAGE_SIZE
#define PAGE_SIZE (1<<12)

struct ebios_dapa {
	uint16_t len;
	uint16_t count;
	uint16_t off;
	uint16_t seg;
	uint64_t lba;
};

#endif /* _COMMON_H_ */
