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

#ifndef DMI_PROCESSOR_H
#define DMI_PROCESSOR_H

#include "stdbool.h"
#define PROCESSOR_SOCKET_DESIGNATION_SIZE       	32
#define PROCESSOR_TYPE_SIZE       	32
#define PROCESSOR_FAMILY_SIZE		32
#define PROCESSOR_MANUFACTURER_SIZE     64
#define PROCESSOR_VERSION_SIZE   	32
#define PROCESSOR_VOLTAGE_SIZE		16	
#define PROCESSOR_STATUS_SIZE		16	
#define PROCESSOR_UPGRADE_SIZE		16
#define PROCESSOR_CACHE_SIZE		16
#define PROCESSOR_SERIAL_SIZE  		32
#define PROCESSOR_ASSET_TAG_SIZE 	32
#define PROCESSOR_PART_NUMBER_SIZE	32
#define PROCESSOR_ID_SIZE		32

#define PROCESSOR_FLAGS_ELEMENTS	32	
/* Intel AP-485 revision 28, table 5 */
static const char *cpu_flags_strings[32]={
                "FPU (Floating-point unit on-chip)", /* 0 */
                "VME (Virtual mode extension)",
                "DE (Debugging extension)",
                "PSE (Page size extension)",
                "TSC (Time stamp counter)",
                "MSR (Model specific registers)",
                "PAE (Physical address extension)",
                "MCE (Machine check exception)",
                "CX8 (CMPXCHG8 instruction supported)",
                "APIC (On-chip APIC hardware supported)",
                NULL, /* 10 */
                "SEP (Fast system call)",
                "MTRR (Memory type range registers)",
                "PGE (Page global enable)",
                "MCA (Machine check architecture)",
                "CMOV (Conditional move instruction supported)",
                "PAT (Page attribute table)",
                "PSE-36 (36-bit page size extension)",
                "PSN (Processor serial number present and enabled)",
                "CLFSH (CLFLUSH instruction supported)",
                NULL, /* 20 */
                "DS (Debug store)",
                "ACPI (ACPI supported)",
                "MMX (MMX technology supported)",
                "FXSR (Fast floating-point save and restore)",
                "SSE (Streaming SIMD extensions)",
                "SSE2 (Streaming SIMD extensions 2)",
                "SS (Self-snoop)",
                "HTT (Hyper-threading technology)",
                "TM (Thermal monitor supported)",
                NULL, /* 30 */
                "PBE (Pending break enabled)" /* 31 */
};

/* this struct have PROCESSOR_FLAGS_ELEMENTS */
/* each bool is associated to the relevant message above */
typedef struct {
bool fpu;
bool vme;
bool de;
bool pse;
bool tsc;
bool msr;
bool pae;
bool mce;
bool cx8;
bool apic;
bool null_10;
bool sep;
bool mtrr;
bool pge;
bool mca;
bool cmov;
bool pat;
bool pse_36;
bool psn;
bool clfsh;
bool null_20;
bool ds;
bool acpi;
bool mmx;
bool fxsr;
bool sse;
bool sse2;
bool ss;
bool htt;
bool tm;
bool null_30;
bool pbe;
}  __attribute__((__packed__)) s_cpu_flags;

typedef struct {
u8 type;
u8 family;
u8 model;
u8 stepping;
u8 minor_stepping;
} __attribute__((__packed__)) s_signature;

typedef struct {
char socket_designation[PROCESSOR_SOCKET_DESIGNATION_SIZE];	
char type[PROCESSOR_TYPE_SIZE];	
char family[PROCESSOR_FAMILY_SIZE];	
char manufacturer[PROCESSOR_MANUFACTURER_SIZE];	
char version[PROCESSOR_VERSION_SIZE];	
float voltage;	
u16  external_clock;
u16  max_speed;
u16  current_speed;
char status[PROCESSOR_STATUS_SIZE];
char upgrade[PROCESSOR_UPGRADE_SIZE];
char cache1[PROCESSOR_CACHE_SIZE];
char cache2[PROCESSOR_CACHE_SIZE];
char cache3[PROCESSOR_CACHE_SIZE];
char serial[PROCESSOR_SERIAL_SIZE];	
char asset_tag[PROCESSOR_ASSET_TAG_SIZE];	
char part_number[PROCESSOR_PART_NUMBER_SIZE];	
char id[PROCESSOR_ID_SIZE];	
s_cpu_flags cpu_flags;
s_signature signature;
} s_processor;

static const char *dmi_processor_type(u8 code)
{
        /* 3.3.5.1 */
        static const char *type[]={
                "Other", /* 0x01 */
                "Unknown",
                "Central Processor",
                "Math Processor",
                "DSP Processor",
                "Video Processor" /* 0x06 */
        };

        if(code>=0x01 && code<=0x06)
                return type[code-0x01];
        return out_of_spec;
}

static const char *dmi_processor_family(u8 code)
{
        /* 3.3.5.2 */
        static const char *family[]={
		NULL, /* 0x00 */
		"Other",
		"Unknown",
		"8086",
		"80286",
		"80386",
		"80486",
		"8087",
		"80287",
		"80387",
		"80487",
		"Pentium",
		"Pentium Pro",
		"Pentium II",
		"Pentium MMX",
		"Celeron",
		"Pentium II Xeon",
		"Pentium III",
		"M1",
		"M2",
		NULL, /* 0x14 */
		NULL,
		NULL,
		NULL, /* 0x17 */
		"Duron",
		"K5",
		"K6",
		"K6-2",
		"K6-3",
		"Athlon",
		"AMD2900",
		"K6-2+",
		"Power PC",
		"Power PC 601",
		"Power PC 603",
		"Power PC 603+",
		"Power PC 604",
		"Power PC 620",
		"Power PC x704",
		"Power PC 750",
		NULL, /* 0x28 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,/* 0x2F */
		"Alpha",
		"Alpha 21064",
		"Alpha 21066",
		"Alpha 21164",
		"Alpha 21164PC",
		"Alpha 21164a",
		"Alpha 21264",
		"Alpha 21364",
		NULL, /* 0x38 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x3F */
		"MIPS",
		"MIPS R4000",
		"MIPS R4200",
		"MIPS R4400",
		"MIPS R4600",
		"MIPS R10000",
		NULL, /* 0x46 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x4F */
		"SPARC",
		"SuperSPARC",
		"MicroSPARC II",
		"MicroSPARC IIep",
		"UltraSPARC",
		"UltraSPARC II",
		"UltraSPARC IIi",
		"UltraSPARC III",
		"UltraSPARC IIIi",
		NULL, /* 0x59 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x5F */
		"68040",
		"68xxx",
		"68000",
		"68010",
		"68020",
		"68030",
		NULL, /* 0x66 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x6F */
		"Hobbit",
		NULL, /* 0x71 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x77 */
		"Crusoe TM5000",
		"Crusoe TM3000",
		"Efficeon TM8000",
		NULL, /* 0x7B */
		NULL,
		NULL,
		NULL,
		NULL, /* 0x7F */
		"Weitek",
		NULL, /* 0x81 */
		"Itanium",
		"Athlon 64",
		"Opteron",
		"Sempron",
		NULL, /* 0x86 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x8F */
		"PA-RISC",
		"PA-RISC 8500",
		"PA-RISC 8000",
		"PA-RISC 7300LC",
		"PA-RISC 7200",
		"PA-RISC 7100LC",
		"PA-RISC 7100",
		NULL, /* 0x97 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0x9F */
		"V30",
		NULL, /* 0xA1 */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0xAF */
		"Pentium III Xeon",
		"Pentium III Speedstep",
		"Pentium 4",
		"Xeon",
		"AS400",
		"Xeon MP",
		"Athlon XP",
		"Athlon MP",
		"Itanium 2",
		"Pentium M",
		NULL, /* 0xBA */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0xC7 */
		"IBM390",
		"G4",
		"G5",
		NULL, /* 0xCB */
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL, /* 0xF9 */
		"i860",
		"i960",
		NULL, /* 0xFC */
		NULL,
		NULL,
		NULL /* 0xFF */
		/* master.mif has values beyond that, but they can't be used for DMI */
        };

        if(family[code]!=NULL) {
                	return family[code];
	}
        return out_of_spec;
}

static const char *dmi_processor_status(u8 code)
{
        static const char *status[]={
                "Unknown", /* 0x00 */
                "Enabled",
                "Disabled By User",
                "Disabled By BIOS",
                "Idle", /* 0x04 */
                "Other" /* 0x07 */
        };

        if(code<=0x04)
                return status[code];
        if(code==0x07)
                return status[0x05];
        return out_of_spec;
}
static const char *dmi_processor_upgrade(u8 code)
{
        /* 3.3.5.5 */
        static const char *upgrade[]={
                "Other", /* 0x01 */
                "Unknown",
                "Daughter Board",
                "ZIF Socket",
                "Replaceable Piggy Back",
                "None",
                "LIF Socket",
                "Slot 1",
                "Slot 2",
                "370-pin Socket",
                "Slot A",
                "Slot M",
                "Socket 423",
                "Socket A (Socket 462)",
                "Socket 478",
                "Socket 754",
                "Socket 940",
                "Socket 939" /* 0x12 */
        };

        if(code>=0x01 && code<=0x11)
                return upgrade[code-0x01];
        return out_of_spec;
}

static void dmi_processor_cache(u16 code, const char *level, u16 ver, char *cache)
{
        if(code==0xFFFF)
        {
                if(ver>=0x0203)
                        sprintf(cache,"Not Provided");
                else
                        sprintf(cache,"No %s Cache", level);
        }
        else
                sprintf(cache,"0x%04X", code);
}


#endif
