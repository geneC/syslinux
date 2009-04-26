#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <stdint.h>

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

void get_label(int label, char** buffer_label);
#endif /* _PARTITION_H_ */
