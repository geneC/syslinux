CC	 = gcc -m32
LD       = ld -m elf_i386
OBJCOPY  = objcopy
OPTFLAGS = -g -Os
INCLUDES = -include code16.h -I. -I.. -I../libfat
CFLAGS	 = -W -Wall -ffreestanding $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -T com16.ld

SRCS     = syslinux.c printf.c conio.c skipatou.c atou.c malloc.c free.c \
	   ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c \
           $(wildcard ../libfat/*.c)
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S .elf .com

VPATH = .:..:../libfat

all: installer

tidy:
	-rm -f *.o *.i *.s *.a .*.d *.elf

clean: tidy
	-rm -f syslinux.com

spotless: clean
	-rm -f *~

installer: syslinux.com

syslinux.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

syslinux.com: syslinux.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) -Wp,-MT,$@,-MD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<
%.s: %.S
	$(CC) $(CFLAGS) -D__ASSEMBLY__ -S -o $@ $<

-include .*.d



