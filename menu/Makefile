CC	= gcc -funsigned-char -W -Wall
CFLAGS  = -march=i386 -Os
AS      = as
LD      = ld
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

