/*
 *  mboot.c
 *
 *  Loader for Multiboot-compliant kernels and modules.
 *
 *  Copyright (C) 2005 Tim Deegan <Tim.Deegan@cl.cam.ac.uk>
 *  Parts based on GNU GRUB, Copyright (C) 2000  Free Software Foundation, Inc.
 *  Parts based on SYSLINUX, Copyright (C) 1994-2005  H. Peter Anvin.
 *  Thanks to Ram Yalamanchili for the ELF section-header loading.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <malloc.h>
#include <consoles.h>
#include <zlib.h>
#include <com32.h>

#include "i386-elf.h"
#include "mb_info.h"
#include "mb_header.h"

#include <klibc/compiler.h> /* For __constructor */

#define MIN(_x, _y) (((_x)<(_y))?(_x):(_y))
#define MAX(_x, _y) (((_x)>(_y))?(_x):(_y))

/* Define this for some more printout */
#undef DEBUG

/* Memory magic numbers */
#define STACK_SIZE      0x20000      /* XXX Could be much smaller */
#define MALLOC_SIZE     0x100000     /* XXX Could be much smaller */
#define MIN_RUN_ADDR    0x10000      /* Lowest address we'll consider using */
#define MEM_HOLE_START  0xa0000      /* Memory hole runs from 640k ... */
#define MEM_HOLE_END    0x100000     /* ... to 1MB */
#define X86_PAGE_SIZE   0x1000

size_t __stack_size = STACK_SIZE;    /* How much stack we'll use */
extern void *__mem_end;              /* Start of malloc() heap */
extern char _end[];                  /* End of static data */

/* Pointer to free memory for loading into: load area is between here
 * and section_addr */
static char *next_load_addr;

/* Memory map for run-time */
typedef struct section section_t;
struct section {
    size_t dest;                     /* Start of run-time allocation */
    char *src;                       /* Current location of data for memmove(),
                                      * or NULL for bzero() */
    size_t size;                     /* Length of allocation */
};
static char *section_addr;
static int section_count;

static size_t max_run_addr;          /* Highest address we'll consider using */
static size_t next_mod_run_addr;     /* Where the next module will be put */

/* File loads are in units of this much */
#define LOAD_CHUNK 0x20000

/* Layout of the input to the 32-bit lidt instruction */
struct lidt_operand {
    unsigned int limit:16;
    unsigned int base:32;
} __attribute__((packed));

/* Magic strings */
static const char version_string[]   = "COM32 Multiboot loader v0.2";
static const char copyright_string[] = "Copyright (C) 2005-2006 Tim Deegan.";
static const char module_separator[] = "---";


/*
 *  Start of day magic, run from __start during library init.
 */

static void __constructor check_version(void)
    /* Check the SYSLINUX version.  Docs say we should be OK from v2.08,
     * but in fact we crash on anything below v2.12 (when libc came in). */
{
    com32sys_t regs_in, regs_out;
    const char *p, *too_old = "Fatal: SYSLINUX image is too old; "
                              "mboot.c32 needs at least version 2.12.\r\n";

    memset(&regs_in, 0, sizeof(regs_in));
    regs_in.eax.l = 0x0001;  /* "Get version" */
    __intcall(0x22, &regs_in, &regs_out);
    if (regs_out.ecx.w[0] >= 0x020c) return;

    /* Pointless: on older versions this print fails too. :( */
    for (p = too_old ; *p ; p++) {
        memset(&regs_in, 0, sizeof(regs_in));
        regs_in.eax.b[1] = 0x02;      /* "Write character" */
        regs_in.edx.b[0] = *p;
        __intcall(0x21, &regs_in, &regs_out);
    }

    __intcall(0x20, &regs_in, &regs_out);  /* "Terminate program" */
}


static void __constructor grab_memory(void)
    /* Runs before init_memory_arena() (com32/lib/malloc.c) to let
     * the malloc() code know how much space it's allowed to use.
     * We don't use malloc() directly, but some of the library code
     * does (zlib, for example). */
{
    /* Find the stack pointer */
    register char * sp;
    asm volatile("movl %%esp, %0" : "=r" (sp));

    /* Initialize the allocation of *run-time* memory: don't let ourselves
     * overwrite the stack during the relocation later. */
    max_run_addr = (size_t) sp - (MALLOC_SIZE + STACK_SIZE);

    /* Move the end-of-memory marker: malloc() will use only memory
     * above __mem_end and below the stack.  We will load files starting
     * at the old __mem_end and working towards the new one, and allocate
     * section descriptors at the top of that area, working down. */
    next_load_addr = __mem_end;
    section_addr = sp - (MALLOC_SIZE + STACK_SIZE);
    section_count = 0;

    /* But be careful not to move it the wrong direction if memory is
     * tight.  Instead we'll fail more gracefully later, when we try to
     * load a file and find that next_load_addr > section_addr. */
    __mem_end = MAX(section_addr, next_load_addr);
}




/*
 *  Run-time memory map functions: allocating and recording allocations.
 */

static int cmp_sections(const void *a, const void *b)
    /* For sorting section descriptors by destination address */
{
    const section_t *sa = a;
    const section_t *sb = b;
    if (sa->dest < sb->dest) return -1;
    if (sa->dest > sb->dest) return 1;
    return 0;
}


static void add_section(size_t dest, char *src, size_t size)
    /* Adds something to the list of sections to relocate. */
{
    section_t *sec;

#ifdef DEBUG
    printf("SECTION: %#8.8x --> %#8.8x (%#x)\n", (size_t) src, dest, size);
#endif

    section_addr -= sizeof (section_t);
    if (section_addr < next_load_addr) {
        printf("Fatal: out of memory allocating section descriptor.\n");
        exit(1);
    }
    sec = (section_t *) section_addr;
    section_count++;

    sec->src = src;
    sec->dest = dest;
    sec->size = size;

    /* Keep the list sorted */
    qsort(sec, section_count, sizeof (section_t), cmp_sections);
}


static size_t place_low_section(size_t size, size_t align)
    /* Find a space in the run-time memory map, below 640K */
{
    int i;
    size_t start;
    section_t *sections = (section_t *) section_addr;

    start = MIN_RUN_ADDR;
    start = (start + (align-1)) & ~(align-1);

    /* Section list is sorted by destination, so can do this in one pass */
    for (i = 0; i < section_count; i++) {
        if (sections[i].dest < start + size) {
            /* Hit the bottom of this section */
            start = sections[i].dest + sections[i].size;
            start = (start + (align-1)) & ~(align-1);
        }
    }
    if (start + size < MEM_HOLE_START) return start;
    else return 0;
}


static size_t place_module_section(size_t size, size_t align)
    /* Find a space in the run-time memory map for this module. */
{
    /* Ideally we'd run through the sections looking for a free space
     * like place_low_section() does, but some OSes (Xen, at least)
     * assume that the bootloader has loaded all the modules
     * consecutively, above the kernel.  So, what we actually do is
     * keep a pointer to the highest address allocated so far, and
     * always allocate modules there. */

    size_t start = next_mod_run_addr;
    start = (start + (align-1)) & ~(align-1);

    if (start + size > max_run_addr) return 0;

    next_mod_run_addr = start + size;
    return start;
}


static void place_kernel_section(size_t start, size_t size)
    /* Allocate run-time space for part of the kernel, checking for
     * sanity.  We assume the kernel isn't broken enough to have
     * overlapping segments. */
{
    /* We always place modules above the kernel */
    next_mod_run_addr = MAX(next_mod_run_addr, start + size);

    if (start > max_run_addr || start + size > max_run_addr) {
        /* Overruns the end of memory */
        printf("Fatal: kernel loads too high (%#8.8x+%#x > %#8.8x).\n",
               start, size, max_run_addr);
        exit(1);
    }
    if (start >= MEM_HOLE_END) {
        /* Above the memory hole: easy */
#ifdef DEBUG
        printf("Placed kernel section (%#8.8x+%#x)\n", start, size);
#endif
        return;
    }
    if (start >= MEM_HOLE_START) {
        /* In the memory hole.  Not so good */
        printf("Fatal: kernel load address (%#8.8x) is in the memory hole.\n",
               start);
        exit(1);
    }
    if (start + size > MEM_HOLE_START) {
        /* Too big for low memory */
        printf("Fatal: kernel (%#8.8x+%#x) runs into the memory hole.\n",
               start, size);
        exit(1);
    }
    if (start < MIN_RUN_ADDR) {
        /* Loads too low */
        printf("Fatal: kernel load address (%#8.8x) is too low (<%#8.8x).\n",
               start, MIN_RUN_ADDR);
        exit(1);
    }
    /* Kernel loads below the memory hole: OK */
#ifdef DEBUG
    printf("Placed kernel section (%#8.8x+%#x)\n", start, size);
#endif
}


static void reorder_sections(void)
    /* Reorders sections into a safe order, where no relocation
     * overwrites the source of a later one.  */
{
    section_t *secs = (section_t *) section_addr;
    section_t tmp;
    int i, j, tries;

#ifdef DEBUG
    printf("Relocations:\n");
    for (i = 0; i < section_count ; i++) {
        printf("    %#8.8x --> %#8.8x (%#x)\n",
               (size_t)secs[i].src, secs[i].dest, secs[i].size);
    }
#endif

    for (i = 0; i < section_count; i++) {
        tries = 0;
    scan_again:
        for (j = i + 1 ; j < section_count; j++) {
            if (secs[j].src != NULL
                && secs[i].dest + secs[i].size > (size_t) secs[j].src
                && secs[i].dest < (size_t) secs[j].src + secs[j].size) {
                /* Would overwrite the source of the later move */
                if (++tries > section_count) {
                    /* Deadlock! */
                    /* XXX Try to break deadlocks? */
                    printf("Fatal: circular dependence in relocations.\n");
                    exit(1);
                }
                /* Swap these sections (using struct copies) */
                tmp = secs[i]; secs[i] = secs[j]; secs[j] = tmp;
                /* Start scanning again from the new secs[i]... */
                goto scan_again;
            }
        }
    }

#ifdef DEBUG
    printf("Relocations:\n");
    for (i = 0; i < section_count ; i++) {
        printf("    %#8.8x --> %#8.8x (%#x)\n",
               (size_t)secs[i].src, secs[i].dest, secs[i].size);
    }
#endif
}


static void init_mmap(struct multiboot_info *mbi)
    /* Get a full memory map from the BIOS to pass to the kernel. */
{
    com32sys_t regs_in, regs_out;
    struct AddrRangeDesc *e820;
    int e820_slots;
    size_t mem_lower, mem_upper, run_addr, mmap_size;
    register size_t sp;

    /* Default values for mem_lower and mem_upper in case the BIOS won't
     * tell us: 640K, and all memory up to the stack. */
    asm volatile("movl %%esp, %0" : "=r" (sp));
    mem_upper = (sp - MEM_HOLE_END) / 1024;
    mem_lower = (MEM_HOLE_START) / 1024;

#ifdef DEBUG
    printf("Requesting memory map from BIOS:\n");
#endif

    /* Ask the BIOS for the full memory map of the machine.  We'll
     * build it in Multiboot format (i.e. with size fields) in the
     * bounce buffer, and then allocate some high memory to keep it in
     * until boot time. */
    e820 = __com32.cs_bounce;
    e820_slots = 0;
    regs_out.ebx.l = 0;

    while(((void *)(e820 + 1)) < __com32.cs_bounce + __com32.cs_bounce_size)
    {
        memset(e820, 0, sizeof (*e820));
        memset(&regs_in, 0, sizeof regs_in);
        e820->size = sizeof(*e820) - sizeof(e820->size);

        /* Ask the BIOS to fill in this descriptor */
        regs_in.eax.l = 0xe820;         /* "Get system memory map" */
        regs_in.ebx.l = regs_out.ebx.l; /* Continuation value from last call */
        regs_in.ecx.l = 20;             /* Size of buffer to write into */
        regs_in.edx.l = 0x534d4150;     /* "SMAP" */
        regs_in.es = SEG(&e820->BaseAddr);
        regs_in.edi.w[0] = OFFS(&e820->BaseAddr);
        __intcall(0x15, &regs_in, &regs_out);

        if ((regs_out.eflags.l & EFLAGS_CF) != 0 && regs_out.ebx.l != 0)
            break;  /* End of map */

        if (((regs_out.eflags.l & EFLAGS_CF) != 0 && regs_out.ebx.l == 0)
            || (regs_out.eax.l != 0x534d4150))
        {
            /* Error */
            printf("Error %x reading E820 memory map: %s.\n",
                   (int) regs_out.eax.b[0],
                   (regs_out.eax.b[0] == 0x80) ? "invalid command" :
                   (regs_out.eax.b[0] == 0x86) ? "not supported" :
                   "unknown error");
            break;
        }

        /* Success */
#ifdef DEBUG
        printf("    %#16.16Lx -- %#16.16Lx : ",
               e820->BaseAddr, e820->BaseAddr + e820->Length);
        switch (e820->Type) {
        case 1: printf("Available\n"); break;
        case 2: printf("Reserved\n"); break;
        case 3: printf("ACPI Reclaim\n"); break;
        case 4: printf("ACPI NVS\n"); break;
        default: printf("? (Reserved)\n"); break;
        }
#endif

        if (e820->Type == 1) {
            if (e820->BaseAddr == 0) {
                mem_lower = MIN(MEM_HOLE_START, e820->Length) / 1024;
            } else if (e820->BaseAddr == MEM_HOLE_END) {
                mem_upper = MIN(0xfff00000, e820->Length) / 1024;
            }
        }

        /* Move to next slot */
        e820++;
        e820_slots++;

        /* Done? */
        if (regs_out.ebx.l == 0)
            break;
    }

    /* Record the simple information in the MBI */
    mbi->flags |= MB_INFO_MEMORY;
    mbi->mem_lower = mem_lower;
    mbi->mem_upper = mem_upper;

    /* Record the full memory map in the MBI */
    if (e820_slots != 0) {
        mmap_size = e820_slots * sizeof(*e820);
        /* Where will it live at run time? */
        run_addr = place_low_section(mmap_size, 1);
        if (run_addr == 0) {
            printf("Fatal: can't find space for the e820 mmap.\n");
            exit(1);
        }
        /* Where will it live now? */
        e820 = (struct AddrRangeDesc *) next_load_addr;
        if (next_load_addr + mmap_size > section_addr) {
            printf("Fatal: out of memory storing the e820 mmap.\n");
            exit(1);
        }
        next_load_addr += mmap_size;
        /* Copy it out of the bounce buffer */
        memcpy(e820, __com32.cs_bounce, mmap_size);
        /* Remember to copy it again at run time */
        add_section(run_addr, (char *) e820, mmap_size);
        /* Record it in the MBI */
        mbi->flags |= MB_INFO_MEM_MAP;
        mbi->mmap_length = mmap_size;
        mbi->mmap_addr = run_addr;
    }
}




/*
 *  Code for loading and parsing files.
 */

static void load_file(char *filename, char **startp, size_t *sizep)
    /* Load a file into memory.  Returns where it is and how big via
     * startp and sizep */
{
    gzFile fp;
    char *start;
    int bsize;

    printf("Loading %s.", filename);

    start = next_load_addr;
    startp[0] = start;
    sizep[0] = 0;

    /* Open the file */
    if ((fp = gzopen(filename, "r")) == NULL) {
        printf("\nFatal: cannot open %s\n", filename);
        exit(1);
    }

    while (next_load_addr + LOAD_CHUNK <= section_addr) {
        bsize = gzread(fp, next_load_addr, LOAD_CHUNK);
        printf("%s",".");

        if (bsize < 0) {
            printf("\nFatal: read error in %s\n", filename);
            gzclose(fp);
            exit(1);
        }

        next_load_addr += bsize;
        sizep[0] += bsize;

        if (bsize < LOAD_CHUNK) {
            printf("%s","\n");
            gzclose(fp);
            return;
        }
    }

    /* Running out of memory.  Try and use up the last bit */
    if (section_addr > next_load_addr) {
        bsize = gzread(fp, next_load_addr, section_addr - next_load_addr);
        printf("%s",".");
    } else {
        bsize = 0;
    }

    if (bsize < 0) {
        gzclose(fp);
        printf("\nFatal: read error in %s\n", filename);
        exit(1);
    }

    next_load_addr += bsize;
    sizep[0] += bsize;

    if (!gzeof(fp)) {
        gzclose(fp);
        printf("\nFatal: out of memory reading %s\n", filename);
        exit(1);
    }

    printf("%s","\n");
    gzclose(fp);
    return;
}


static size_t load_kernel(struct multiboot_info *mbi, char *cmdline)
    /* Load a multiboot/elf32 kernel and allocate run-time memory for it.
     * Returns the kernel's entry address.  */
{
    unsigned int i;
    char *load_addr;                  /* Where the image was loaded */
    size_t load_size;                              /* How big it is */
    char *seg_addr;                   /* Where a segment was loaded */
    size_t seg_size, bss_size;                     /* How big it is */
    size_t run_addr, run_size;            /* Where it should be put */
    size_t shdr_run_addr;
    char *p;
    Elf32_Ehdr *ehdr;
    Elf32_Phdr *phdr;
    Elf32_Shdr *shdr;
    struct multiboot_header *mbh;

    printf("Kernel: %s\n", cmdline);

    load_addr = 0;
    load_size = 0;
    p = strchr(cmdline, ' ');
    if (p != NULL) *p = 0;
    load_file(cmdline, &load_addr, &load_size);
    if (load_size < 12) {
        printf("Fatal: %s is too short to be a multiboot kernel.",
               cmdline);
        exit(1);
    }
    if (p != NULL) *p = ' ';


    /* Look for a multiboot header in the first 8k of the file */
    for (i = 0; i <= MIN(load_size - 12, MULTIBOOT_SEARCH - 12); i += 4)
    {
        mbh = (struct multiboot_header *)(load_addr + i);
        if (mbh->magic != MULTIBOOT_MAGIC
            || ((mbh->magic+mbh->flags+mbh->checksum) & 0xffffffff))
        {
            /* Not a multiboot header */
            continue;
        }
        if (mbh->flags & (MULTIBOOT_UNSUPPORTED | MULTIBOOT_VIDEO_MODE)) {
            /* Requires options we don't support */
            printf("Fatal: Kernel requires multiboot options "
                   "that I don't support: %#x.\n",
                   mbh->flags & (MULTIBOOT_UNSUPPORTED|MULTIBOOT_VIDEO_MODE));
            exit(1);
        }

        /* This kernel will do: figure out where all the pieces will live */

        if (mbh->flags & MULTIBOOT_AOUT_KLUDGE) {

            /* Use the offsets in the multiboot header */
#ifdef DEBUG
            printf("Using multiboot header.\n");
#endif

            /* Where is the code in the loaded file? */
            seg_addr = ((char *)mbh) - (mbh->header_addr - mbh->load_addr);

            /* How much code is there? */
            run_addr = mbh->load_addr;
            if (mbh->load_end_addr != 0)
                seg_size = mbh->load_end_addr - mbh->load_addr;
            else
                seg_size = load_size - (seg_addr - load_addr);

            /* How much memory will it take up? */
            if (mbh->bss_end_addr != 0)
                run_size = mbh->bss_end_addr - mbh->load_addr;
            else
                run_size = seg_size;

            if (seg_size > run_size) {
                printf("Fatal: can't put %i bytes of kernel into %i bytes "
                       "of memory.\n", seg_size, run_size);
                exit(1);
            }
            if (seg_addr + seg_size > load_addr + load_size) {
                printf("Fatal: multiboot load segment runs off the "
                       "end of the file.\n");
                exit(1);
            }

            /* Does it fit where it wants to be? */
            place_kernel_section(run_addr, run_size);

            /* Put it on the relocation list */
            if (seg_size < run_size) {
                /* Set up the kernel BSS too */
                if (seg_size > 0)
                    add_section(run_addr, seg_addr, seg_size);
                bss_size = run_size - seg_size;
                add_section(run_addr + seg_size, NULL, bss_size);
            } else {
                /* No BSS */
                add_section(run_addr, seg_addr, run_size);
            }

            /* Done. */
            return mbh->entry_addr;

        } else {

            /* Now look for an ELF32 header */
            ehdr = (Elf32_Ehdr *)load_addr;
            if (*(unsigned long *)ehdr != 0x464c457f
                || ehdr->e_ident[EI_DATA] != ELFDATA2LSB
                || ehdr->e_ident[EI_CLASS] != ELFCLASS32
                || ehdr->e_machine != EM_386)
            {
                printf("Fatal: kernel has neither ELF32/x86 nor multiboot load"
                       " headers.\n");
                exit(1);
            }
            if (ehdr->e_phoff + ehdr->e_phnum*sizeof (*phdr) > load_size) {
                printf("Fatal: malformed ELF header overruns EOF.\n");
                exit(1);
            }
            if (ehdr->e_phnum <= 0) {
                printf("Fatal: ELF kernel has no program headers.\n");
                exit(1);
            }

#ifdef DEBUG
            printf("Using ELF header.\n");
#endif

            if (ehdr->e_type != ET_EXEC
                || ehdr->e_version != EV_CURRENT
                || ehdr->e_phentsize != sizeof (Elf32_Phdr)) {
                printf("Warning: funny-looking ELF header.\n");
            }
            phdr = (Elf32_Phdr *)(load_addr + ehdr->e_phoff);

            /* Obey the program headers to load the kernel */
            for(i = 0; i < ehdr->e_phnum; i++) {

                /* How much is in this segment? */
                run_size = phdr[i].p_memsz;
                if (phdr[i].p_type != PT_LOAD)
                    seg_size = 0;
                else
                    seg_size = (size_t)phdr[i].p_filesz;

                /* Where is it in the loaded file? */
                seg_addr = load_addr + phdr[i].p_offset;
                if (seg_addr + seg_size > load_addr + load_size) {
                    printf("Fatal: ELF load segment runs off the "
                           "end of the file.\n");
                    exit(1);
                }

                /* Skip segments that don't take up any memory */
                if (run_size == 0) continue;

                /* Place the segment where it wants to be */
                run_addr = phdr[i].p_paddr;
                place_kernel_section(run_addr, run_size);

                /* Put it on the relocation list */
                if (seg_size < run_size) {
                    /* Set up the kernel BSS too */
                    if (seg_size > 0)
                        add_section(run_addr, seg_addr, seg_size);
                    bss_size = run_size - seg_size;
                    add_section(run_addr + seg_size, NULL, bss_size);
                } else {
                    /* No BSS */
                    add_section(run_addr, seg_addr, run_size);
                }
            }

            if (ehdr->e_shoff != 0) {
#ifdef DEBUG
                printf("Loading ELF section table.\n");
#endif
                /* Section Header */
                shdr = (Elf32_Shdr *)(load_addr + ehdr->e_shoff);

                /* Section Header Table size */
                run_size = ehdr->e_shentsize * ehdr->e_shnum;
                shdr_run_addr = place_module_section(run_size, 0x1000);
                if (shdr_run_addr == 0) {
                    printf("Warning: Not enough memory to load the "
                           "section table.\n");
                    return ehdr->e_entry;
                }
                add_section(shdr_run_addr, (void*) shdr, run_size);

                /* Load section tables not loaded thru program segments */
                for (i = 0; i < ehdr->e_shnum; i++) {
                   /* This case is when this section is already included in
                    * program header or it's 0 size, so no need to load */
                   if (shdr[i].sh_addr != 0 || !shdr[i].sh_size)
                       continue;

                   if (shdr[i].sh_addralign == 0)
                       shdr[i].sh_addralign = 1;

                   run_addr = place_module_section(shdr[i].sh_size,
                                                   shdr[i].sh_addralign);
                   if (run_addr == 0) {
                       printf("Warning: Not enough memory to load "
                              "section %d.\n", i);
                       return ehdr->e_entry;
                   }
                   shdr[i].sh_addr = run_addr;
                   add_section(run_addr,
                               (void*) (shdr[i].sh_offset + load_addr),
                               shdr[i].sh_size);
                }

                mbi->flags |= MB_INFO_ELF_SHDR;
                mbi->syms.e.num = ehdr->e_shnum;
                mbi->syms.e.size = ehdr->e_shentsize;
                mbi->syms.e.shndx = ehdr->e_shstrndx;
                mbi->syms.e.addr = shdr_run_addr;
#ifdef DEBUG
                printf("Section information: shnum: %lu, entSize: %lu, "
                       "shstrndx: %lu, addr: 0x%lx\n",
                       mbi->syms.e.num, mbi->syms.e.size,
                       mbi->syms.e.shndx, mbi->syms.e.addr);
#endif
            }

            /* Done! */
            return ehdr->e_entry;
        }
    }

    /* This is not a multiboot kernel */
    printf("Fatal: not a multiboot kernel.\n");
    exit(1);
}



static void load_module(struct mod_list *mod, char *cmdline)
    /* Load a multiboot module and allocate a memory area for it */
{
    char *load_addr, *p;
    size_t load_size, run_addr;

    printf("Module: %s\n", cmdline);

    load_addr = 0;
    load_size = 0;
    p = strchr(cmdline, ' ');
    if (p != NULL) *p = 0;
    load_file(cmdline, &load_addr, &load_size);
    if (p != NULL) *p = ' ';

    /* Decide where it's going to live */
    run_addr = place_module_section(load_size, X86_PAGE_SIZE);
    if (run_addr == 0) {
        printf("Fatal: can't find space for this module.\n");
        exit(1);
    }
    add_section(run_addr, load_addr, load_size);

    /* Remember where we put it */
    mod->mod_start = run_addr;
    mod->mod_end = run_addr + load_size;
    mod->pad = 0;

#ifdef DEBUG
    printf("Placed module (%#8.8x+%#x)\n", run_addr, load_size);
#endif
}




/*
 *  Code for shuffling sections into place and booting the new kernel
 */

static void trampoline_start(section_t *secs, int sec_count,
                             size_t mbi_run_addr, size_t entry)
    /* Final shuffle-and-boot code.  Running on the stack; no external code
     * or data can be relied on. */
{
    int i;
    struct lidt_operand idt;

    /* SYSLINUX has set up SS, DS and ES as 32-bit 0--4G data segments,
     * but doesn't specify FS and GS.  Multiboot wants them all to be
     * the same, so we'd better do that before we overwrite the GDT. */
    asm volatile("movl %ds, %ecx; movl %ecx, %fs; movl %ecx, %gs");

    /* Turn off interrupts */
    asm volatile("cli");

    /* SYSLINUX has set up an IDT at 0x100000 that does all the
     * comboot calls, and we're about to overwrite it.  The Multiboot
     * spec says that the kernel must set up its own IDT before turning
     * on interrupts, but it's still entitled to use BIOS calls, so we'll
     * put the IDT back to the BIOS one at the base of memory. */
    idt.base = 0;
    idt.limit = 0x800;
    asm volatile("lidt %0" : : "m" (idt));

    /* Now, shuffle the sections */
    for (i = 0; i < sec_count; i++) {
        if (secs[i].src == NULL) {
            /* asm bzero() code from com32/lib/memset.c */
            char *q = (char *) secs[i].dest;
            size_t nl = secs[i].size >> 2;
            asm volatile("cld ; rep ; stosl ; movl %3,%0 ; rep ; stosb"
                         : "+c" (nl), "+D" (q)
                         : "a" (0x0U), "r" (secs[i].size & 3));
        } else {
            /* asm memmove() code from com32/lib/memmove.c */
            const char *p = secs[i].src;
            char *q = (char *) secs[i].dest;
            size_t n = secs[i].size;
            if ( q < p ) {
                asm volatile("cld ; rep ; movsb"
                             : "+c" (n), "+S" (p), "+D" (q));
            } else {
                p += (n-1);
                q += (n-1);
                asm volatile("std ; rep ; movsb"
                             : "+c" (n), "+S" (p), "+D" (q));
            }
        }
    }

    /* Now set up the last tiny bit of Multiboot environment.
     * A20 is already enabled.
     * CR0 already has PG cleared and PE set.
     * EFLAGS already has VM and IF cleared.
     * ESP is the kernels' problem.
     * GDTR is the kernel's problem.
     * CS is already a 32-bit, 0--4G code segments.
     * DS, ES, FS and GS are already 32-bit, 0--4G data segments.
     *
     * EAX must be 0x2badb002 and EBX must point to the MBI when we jump. */

    asm volatile ("jmp %*%2"
                  : : "a" (0x2badb002), "b" (mbi_run_addr), "cdSDm" (entry));
}
static void trampoline_end(void) {}


static void boot(size_t mbi_run_addr, size_t entry)
    /* Tidy up SYSLINUX, shuffle memory and boot the kernel */
{
    com32sys_t regs;
    section_t *tr_sections;
    void (*trampoline)(section_t *, int, size_t, size_t);
    size_t trampoline_size;

    /* Make sure the relocations are safe. */
    reorder_sections();

    /* Copy the shuffle-and-boot code and the array of relocations
     * onto the memory we previously used for malloc() heap.  This is
     * safe because it's not the source or the destination of any
     * copies, and there'll be no more library calls after the copy. */

    tr_sections = ((section_t *) section_addr) + section_count;
    trampoline = (void *) (tr_sections + section_count);
    trampoline_size = (void *)&trampoline_end - (void *)&trampoline_start;

#ifdef DEBUG
    printf("tr_sections:     %p\n"
           "trampoline:      %p\n"
           "trampoline_size: %#8.8x\n"
           "max_run_addr:    %#8.8x\n",
           tr_sections, trampoline, trampoline_size, max_run_addr);
#endif

    printf("Booting: MBI=%#8.8x, entry=%#8.8x\n", mbi_run_addr, entry);

    memmove(tr_sections, section_addr, section_count * sizeof (section_t));
    memmove(trampoline, trampoline_start, trampoline_size);

    /* Tell SYSLINUX to clean up */
    memset(&regs, 0, sizeof regs);
    regs.eax.l = 0x000c; /* "Perform final cleanup" */
    regs.edx.l = 0;      /* "Normal cleanup" */
    __intcall(0x22, &regs, NULL);

    /* Into the unknown */
    trampoline(tr_sections, section_count, mbi_run_addr, entry);
}


int main(int argc, char **argv)
    /* Parse the command-line and invoke loaders */
{
    struct multiboot_info *mbi;
    struct mod_list *modp;
    int modules, num_append_args;
    int mbi_reloc_offset;
    char *p;
    size_t mbi_run_addr, mbi_size, entry;
    int i;

    /* Say hello */
    console_ansi_std();
    printf("%s.  %s\n", version_string, copyright_string);

    if (argc < 2 || !strcmp(argv[1], module_separator)) {
        printf("Fatal: No kernel filename!\n");
        exit(1);
    }

#ifdef DEBUG
    printf("_end:           %p\n"
           "argv[1]:        %p\n"
           "next_load_addr: %p\n"
           "section_addr    %p\n"
           "__mem_end:      %p\n"
           "argv[0]:        %p\n",
           &_end, argv[1], next_load_addr, section_addr, __mem_end, argv[0]);
#endif

    /* How much space will the MBI need? */
    modules = 0;
    mbi_size = sizeof(struct multiboot_info) + strlen(version_string) + 5;
    for (i = 1 ; i < argc ; i++) {
        if (!strcmp(argv[i], module_separator)) {
            modules++;
            mbi_size += sizeof(struct mod_list) + 1;
        } else {
            mbi_size += strlen(argv[i]) + 1;
        }
    }

    /* Allocate space in the load buffer for the MBI, all the command
     * lines, and all the module details. */
    mbi = (struct multiboot_info *)next_load_addr;
    next_load_addr += mbi_size;
    if (next_load_addr > section_addr) {
        printf("Fatal: out of memory allocating for boot metadata.\n");
        exit(1);
    }
    memset(mbi, 0, sizeof (struct multiboot_info));
    p = (char *)(mbi + 1);
    mbi->flags = MB_INFO_CMDLINE | MB_INFO_BOOT_LOADER_NAME;

    /* Figure out the memory map.
     * N.B. Must happen before place_section() is called */
    init_mmap(mbi);

    mbi_run_addr = place_low_section(mbi_size, 4);
    if (mbi_run_addr == 0) {
        printf("Fatal: can't find space for the MBI!\n");
        exit(1);
    }
    mbi_reloc_offset = (size_t)mbi - mbi_run_addr;
    add_section(mbi_run_addr, (void *)mbi, mbi_size);

    /* Module info structs */
    modp = (struct mod_list *) (((size_t)p + 3) & ~3);
    if (modules > 0) mbi->flags |= MB_INFO_MODS;
    mbi->mods_count = modules;
    mbi->mods_addr = ((size_t)modp) - mbi_reloc_offset;
    p = (char *)(modp + modules);

    /* Append cmdline args show up in the beginning, append these 
     * to kernel cmdline later on */
    for (i = 1; i < argc; i++) {
        if (strchr(argv[i], '=') != NULL) {
            continue;
        }
        break;
    }

    /* Command lines: first kernel, then modules */
    mbi->cmdline = ((size_t)p) - mbi_reloc_offset;
    modules = 0;
    num_append_args = i-1;
    
    for (; i < argc ; i++) {
        if (!strcmp(argv[i], module_separator)) {
            /* Add append args to kernel cmdline */
            if (modules == 0 && num_append_args) {
                int j;
                for (j = 1; j < num_append_args+1; j++) {
                    strcpy(p, argv[j]);
                    p += strlen(argv[j]);
                    *p++ = ' ';
                }
            }
            *p++ = '\0';
            modp[modules++].cmdline = ((size_t)p) - mbi_reloc_offset;
        } else {
            strcpy(p, argv[i]);
            p += strlen(argv[i]);
            *p++ = ' ';
        }
    }
    *p++ = '\0';

    /* Bootloader ID */
    strcpy(p, version_string);
    mbi->boot_loader_name = ((size_t)p) - mbi_reloc_offset;
    p += strlen(version_string) + 1;

    /* Now, do all the loading, and boot it */
    entry = load_kernel(mbi, (char *)(mbi->cmdline + mbi_reloc_offset));
    for (i=0; i<modules; i++) {
        load_module(&(modp[i]), (char *)(modp[i].cmdline + mbi_reloc_offset));
    }
    boot(mbi_run_addr, entry);

    return 1;
}

/*
 *  EOF
 */
