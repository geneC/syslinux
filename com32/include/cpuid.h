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

#ifndef CPUID_H
#define CPUID_H

#include "stdbool.h"
#include "cpufeature.h"
#define u32 unsigned int
#define __u32 u32
#define __u16 unsigned short
#define __u8  unsigned char
#define PAGE_SIZE 4096

#define CPU_MODEL_SIZE  48
#define CPU_VENDOR_SIZE 48

typedef struct {
        __u32 l;
        __u32 h;
} __u64;

typedef struct {
	bool fpu; /* Onboard FPU */
	bool vme; /* Virtual Mode Extensions */
	bool de;  /* Debugging Extensions */
	bool pse; /* Page Size Extensions */
	bool tsc; /* Time Stamp Counter */
	bool msr; /* Model-Specific Registers, RDMSR, WRMSR */
	bool pae; /* Physical Address Extensions */
	bool mce; /* Machine Check Architecture */
	bool cx8; /* CMPXCHG8 instruction */
	bool apic;/* Onboard APIC */
	bool sep; /* SYSENTER/SYSEXIT */
	bool mtrr;/* Memory Type Range Registers */
	bool pge; /* Page Global Enable */
	bool mca; /* Machine Check Architecture */
	bool cmov;/* CMOV instruction (FCMOVCC and FCOMI too if FPU present) */
	bool pat; /* Page Attribute Table */
	bool pse_36; /* 36-bit PSEs */
	bool psn; /* Processor serial number */
	bool clflsh; /* Supports the CLFLUSH instruction */
	bool dts; /* Debug Trace Store */
	bool acpi;/* ACPI via MSR */
	bool mmx; /* Multimedia Extensions */
	bool fxsr;/* FXSAVE and FXRSTOR instructions (fast save and restore */
                  /* of FPU context), and CR4.OSFXSR available */
	bool sse; /* Streaming SIMD Extensions */
	bool sse2;/* Streaming SIMD Extensions 2*/
	bool ss;  /* CPU self snoop */
	bool htt; /* Hyper-Threading */
	bool acc; /* Automatic clock control */
	bool syscall; /* SYSCALL/SYSRET */
	bool mp;  /* MP Capable. */
	bool nx;  /* Execute Disable */
	bool mmxext;  /* AMD MMX extensions */
	bool lm;  /* Long Mode (x86-64) */
	bool nowext;/* AMD 3DNow! extensions */
	bool now;   /* 3DNow! */
	bool smp;  /* A smp configuration has been found*/
}  __attribute__((__packed__)) s_cpu_flags;

typedef struct {
char vendor[CPU_VENDOR_SIZE];
__u8 vendor_id;
__u8 family;
char model[CPU_MODEL_SIZE];
__u8 model_id;
__u8 stepping;
s_cpu_flags flags;
} s_cpu;

/**********************************************************************************/
/**********************************************************************************/
/* From this point this is some internal stuff mainly taken from the linux kernel */
/**********************************************************************************/
/**********************************************************************************/

/*
 * EFLAGS bits
 */
#define X86_EFLAGS_CF   0x00000001 /* Carry Flag */
#define X86_EFLAGS_PF   0x00000004 /* Parity Flag */
#define X86_EFLAGS_AF   0x00000010 /* Auxillary carry Flag */
#define X86_EFLAGS_ZF   0x00000040 /* Zero Flag */
#define X86_EFLAGS_SF   0x00000080 /* Sign Flag */
#define X86_EFLAGS_TF   0x00000100 /* Trap Flag */
#define X86_EFLAGS_IF   0x00000200 /* Interrupt Flag */
#define X86_EFLAGS_DF   0x00000400 /* Direction Flag */
#define X86_EFLAGS_OF   0x00000800 /* Overflow Flag */
#define X86_EFLAGS_IOPL 0x00003000 /* IOPL mask */
#define X86_EFLAGS_NT   0x00004000 /* Nested Task */
#define X86_EFLAGS_RF   0x00010000 /* Resume Flag */
#define X86_EFLAGS_VM   0x00020000 /* Virtual Mode */
#define X86_EFLAGS_AC   0x00040000 /* Alignment Check */
#define X86_EFLAGS_VIF  0x00080000 /* Virtual Interrupt Flag */
#define X86_EFLAGS_VIP  0x00100000 /* Virtual Interrupt Pending */
#define X86_EFLAGS_ID   0x00200000 /* CPUID detection flag */

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NSC 8
#define X86_VENDOR_NUM 9
#define X86_VENDOR_UNKNOWN 0xff

static inline int test_bit(int nr, const volatile unsigned long *addr)
{
        return ((1UL << (nr & 31)) & (addr[nr >> 5])) != 0;
}

#define cpu_has(c, bit)                test_bit(bit, (c)->x86_capability)

/*
 *  CPU type and hardware bug flags. Kept separately for each CPU.
 *  Members of this structure are referenced in head.S, so think twice
 *  before touching them. [mj]
 */

struct cpuinfo_x86 {
        __u8    x86;            /* CPU family */
        __u8    x86_vendor;     /* CPU vendor */
        __u8    x86_model;
        __u8    x86_mask;
        char    wp_works_ok;    /* It doesn't on 386's */
        char    hlt_works_ok;   /* Problems on some 486Dx4's and old 386's */
        char    hard_math;
        char    rfu;
        int     cpuid_level;    /* Maximum supported CPUID level, -1=no CPUID */
        unsigned long   x86_capability[NCAPINTS];
        char    x86_vendor_id[16];
        char    x86_model_id[64];
        int     x86_cache_size;  /* in KB - valid for CPUS which support this
                                    call  */
        int     x86_cache_alignment;    /* In bytes */
        char    fdiv_bug;
        char    f00f_bug;
        char    coma_bug;
        char    pad0;
        int     x86_power;
        unsigned long loops_per_jiffy;
#ifdef CONFIG_SMP
        cpumask_t llc_shared_map;       /* cpus sharing the last level cache */
#endif
        unsigned char x86_max_cores;    /* cpuid returned max cores value */
        unsigned char booted_cores;     /* number of cores as seen by OS */
        unsigned char apicid;
} __attribute__((__packed__));
#endif

struct cpu_model_info {
        int vendor;
        int family;
        char *model_names[16];
};

/* attempt to consolidate cpu attributes */
struct cpu_dev {
        char    * c_vendor;

        /* some have two possibilities for cpuid string */
        char    * c_ident[2];

        struct          cpu_model_info c_models[4];

        void            (*c_init)(struct cpuinfo_x86 * c);
        void            (*c_identify)(struct cpuinfo_x86 * c);
        unsigned int    (*c_size_cache)(struct cpuinfo_x86 * c, unsigned int size);
};

/*
 * Generic CPUID function
 * clear %ecx since some cpus (Cyrix MII) do not set or clear %ecx
 * resulting in stale register contents being returned.
 */
static inline void cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
        __asm__("cpuid"
                : "=a" (*eax),
                  "=b" (*ebx),
                  "=c" (*ecx),
                  "=d" (*edx)
                : "0" (op), "c"(0));
}

/*
 * Structure definitions for SMP machines following the
 * Intel Multiprocessing Specification 1.1 and 1.4.
 */

/*
 * This tag identifies where the SMP configuration
 * information is.
 */

#define SMP_MAGIC_IDENT (('_'<<24)|('P'<<16)|('M'<<8)|'_')

struct intel_mp_floating
{
        char mpf_signature[4];          /* "_MP_"                       */
        unsigned long mpf_physptr;      /* Configuration table address  */
        unsigned char mpf_length;       /* Our length (paragraphs)      */
        unsigned char mpf_specification;/* Specification version        */
        unsigned char mpf_checksum;     /* Checksum (makes sum 0)       */
        unsigned char mpf_feature1;     /* Standard or configuration ?  */
        unsigned char mpf_feature2;     /* Bit7 set for IMCR|PIC        */
        unsigned char mpf_feature3;     /* Unused (0)                   */
        unsigned char mpf_feature4;     /* Unused (0)                   */
        unsigned char mpf_feature5;     /* Unused (0)                   */
};


extern void get_cpu_vendor(struct cpuinfo_x86 *c);
extern void detect_cpu(s_cpu *cpu);
