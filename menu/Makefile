gcc_ok   = $(shell if gcc $(1) -c -x c /dev/null -o /dev/null 2>/dev/null; \
	           then echo $(1); else echo $(2); fi)

M32     := $(call gcc_ok,-m32,)
ALIGN   := $(call gcc_ok,-falign-functions=0 -falign-jumps=0 -falign-loops=0,-malign-functions=0 -malign-jumps=0 -malign-loops=0)

CC	= gcc $(M32) -funsigned-char
CFLAGS  = -g -W -Wall -march=i386 $(ALIGN) -Os
AS      = as
LD      = ld -m elf_i386
OBJCOPY = objcopy

.SUFFIXES: .c .s .s16 .o16 .elf .com

.c.s:
	$(CC) $(CFLAGS) -S -o $@ $<

.s.s16:
	echo '.code16gcc' | cat - $< > $@

.s16.o16:
	$(AS) -o $@ $<

.elf.com:
	$(OBJCOPY) -O binary $< $@


all : menu.com

menu.elf: startup.o16 main.o16 biosio.o16 string.o16 menu.o16 syslinux.o16
	$(LD) -T com16.ld -o $@ $^

startup.s16: startup.S16
	$(CC) $(CFLAGS) -x assembler-with-cpp -E -o $@ $<

clean:
	-rm -f *.s *.s16 *.o16 *.elf *.com

spotless: clean
	-rm -f *~ \#*

distclean: spotless
	-rm -f main.c

