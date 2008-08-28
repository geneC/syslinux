#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <console.h>
#include <com32.h>
#include <syslinux/loadfile.h>

#define COM32_LOAD_ADDR ((void*)0x101000)
#define STACK_RED_ZONE  0x1000

static inline void error(const char *msg)
{
    fputs(msg, stderr);
}

static void shift_cmdline(struct com32_sys_args *com32)
{
    char *p;

    /* Skip leading whitespace */
    for (p = com32->cs_cmdline; *p != '\0' && *p == ' '; p++)
        ;

    /* Skip first word */
    for (; *p != '\0' && *p != ' '; p++)
        ;

    /* Skip whitespace after first word */
    for (; *p != '\0' && *p == ' '; p++)
        ;

    com32->cs_cmdline = p;
}

static __noreturn reloc_entry(void *ptr, size_t len, uintptr_t entry_esp, uintptr_t module_esp)
{
    size_t stack_frame_size = sizeof(struct com32_sys_args) + 4;
    struct com32_sys_args *com32;

    /* Copy module to load address */
    memcpy(COM32_LOAD_ADDR, ptr, len);

    /* Copy stack frame onto module stack */
    module_esp = (module_esp - stack_frame_size) & ~15;
    memcpy((void*)module_esp, (void*)entry_esp, stack_frame_size);

    /* Fix up command line */
    com32 = (struct com32_sys_args*)((char*)module_esp + 4);
    shift_cmdline(com32);

    /* Invoke module with stack set up */
    asm volatile (
            "movl %0, %%esp\n\t"
            "jmp *%%ecx"
            : : "r"(module_esp), "c"(COM32_LOAD_ADDR)
    );
    for(;;); /* shut the compiler up */
}

static inline __noreturn reloc(void *ptr, size_t len)
{
    extern uintptr_t __entry_esp;
    extern char _start[], _end[];
    size_t total_size = _end - _start;
    __noreturn (*success_fn)(void*, size_t, uintptr_t, uintptr_t);
    uint32_t esp;
    char *dest;

    /* Calculate relocation address, preserve current stack */
    asm volatile ("movl %%esp, %0\n\t" : "=m"(esp));
    dest = (char*)((esp - STACK_RED_ZONE - total_size) & ~3);

    /* Calculate entry point in relocated code */
    success_fn = (void*)(dest + ((char*)reloc_entry - _start));

    /* Copy all sections to relocation address */
    printf("Relocating %d bytes from %p to %p\n", total_size, _start, dest);
    memcpy(dest, _start, total_size);

    success_fn(ptr, len, __entry_esp, (uintptr_t)dest);
}

int main(int argc, char *argv[])
{
    void *data;
    size_t data_len;

    openconsole(&dev_null_r, &dev_stdcon_w);

    if (argc < 2) {
        error("Usage: gdbstub.c32 com32_file arguments...\n");
        return 1;
    }

    if (loadfile(argv[1], &data, &data_len)) {
        error("Unable to load file\n");
        return 1;
    }

    /* No more lib calls after this point */
    reloc(data, data_len);
}
