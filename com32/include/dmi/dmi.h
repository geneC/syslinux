/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef DMI_H
#define DMI_H

#define u32 unsigned int
#define u16 unsigned short
#define u8  unsigned char
#define PAGE_SIZE 4096

typedef struct {
        u32 l;
        u32 h;
} u64;

static const char *out_of_spec = "<OUT OF SPEC>";
static const char *bad_index = "<BAD INDEX>";

#define WORD(x) (u16)(*(const u16 *)(x))
#define DWORD(x) (u32)(*(const u32 *)(x))
#define QWORD(x) (*(const u64 *)(x))

enum {DMI_TABLE_PRESENT = 100, ENODMITABLE};

#include "dmi_bios.h"
#include "dmi_system.h"
#include "dmi_base_board.h"
#include "dmi_chassis.h"
#include "dmi_processor.h"
#include "dmi_memory.h"
#include "dmi_battery.h"

extern char display_line;
#define moreprintf(...) do { display_line++; if (display_line == 24) { char tempbuf[10]; display_line=0; printf("Press enter to continue"); fgets(tempbuf, sizeof tempbuf, stdin);}  printf ( __VA_ARGS__); } while (0);

typedef struct {
u16 num;
u16 len;
u16 ver;
u32 base;
u16 major_version;
u16 minor_version;
} dmi_table;



struct dmi_header
{
        u8 type;
        u8 length;
        u16 handle;
        u8 *data;
};

typedef struct {
	 s_bios bios;
	 s_system system;
	 s_base_board base_board;
	 s_chassis chassis;
	 s_processor processor;
	 s_battery battery;
	 s_memory memory[32];
	 int memory_count;
	 dmi_table dmitable;
} s_dmi;

void to_dmi_header(struct dmi_header *h, u8 *data);
void dmi_bios_runtime_size(u32 code, s_dmi *dmi);
const char *dmi_string(struct dmi_header *dm, u8 s);
inline int dmi_checksum(u8 *buf);
void parse_dmitable(s_dmi *dmi);
void dmi_decode(struct dmi_header *h, u16 ver, s_dmi *dmi);
int dmi_iterate(s_dmi *dmi);

/* dmi_utils.c */
void display_bios_characteristics(s_dmi *dmi);
void display_base_board_features(s_dmi *dmi);
void display_processor_flags(s_dmi *dmi);
#endif
