#ifndef _UTIL_H_
#define _UTIL_H_

#include <com32.h>

#define PARTITION_TABLES_OFFSET	0x1be
/* A DOS partition table entry */
struct part_entry {
	uint8_t active_flag;		/* 0x80 if "active" */
	uint8_t start_head;
	uint8_t start_sect;
	uint8_t start_cyl;
	uint8_t ostype;
	uint8_t end_head;
	uint8_t end_sect;
	uint8_t end_cyl;
	uint32_t start_lba;
	uint32_t length;
} __attribute__((packed));

int int13_retry(const com32sys_t *inreg, com32sys_t *outreg);
void get_error(const int, char**);
void get_label(int label, char** buffer_label);

#endif /* _UTIL_H_ */
