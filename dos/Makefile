CC	 = gcc -m32 -mregparm=3 -DREGPARM=3
LD       = ld -m elf_i386
OBJCOPY  = objcopy
OPTFLAGS = -g -Os -march=i386 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -fomit-frame-pointer
INCLUDES = -include code16.h -I. -I.. -I../libfat
CFLAGS	 = -W -Wall -ffreestanding $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -T com16.ld
AR       = ar
RANLIB   = ranlib

SRCS     = syslinux.c \
	   ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c \
           $(wildcard ../libfat/*.c)
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))
LIBOBJS	 = conio.o memcpy.o memset.o  skipatou.o atou.o malloc.o free.o \
	   argv.o printf.o 

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

syslinux.elf: $(OBJS) libcom.a
	$(LD) $(LDFLAGS) -o $@ $^

libcom.a: $(LIBOBJS)
	-rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

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



