TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32       := $(call gcc_ok,-m32,) $(call gcc_ok,-ffreestanding,) \
	$(call gcc_ok,-fno-stack-protector,) \
	$(call gcc_ok,-fno-top-level-reorder,$(call gcc_ok,-fno-unit-at-a-time))

CC	 = gcc
LD       = ld -m elf_i386
OBJCOPY  = objcopy
OPTFLAGS = -g -Os -march=i386 -falign-functions=0 -falign-jumps=0 -falign-loops=0 -fomit-frame-pointer
INCLUDES = -include code16.h -nostdinc -iwithprefix include \
	   -I. -I.. -I../libfat -I ../libinstaller
CFLAGS	 = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -msoft-float $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -T com16.ld
AR       = ar
RANLIB   = ranlib
LIBGCC  := $(shell $(CC) --print-libgcc)

SRCS     = syslinux.c \
	   ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c ../mbr_bin.c \
           $(wildcard ../libfat/*.c)
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))
LIBOBJS	 = conio.o memcpy.o memset.o  skipatou.o atou.o malloc.o free.o \
	   argv.o printf.o __divdi3.o __udivmoddi4.o

.SUFFIXES: .c .o .i .s .S .elf .com

VPATH = .:..:../libfat:../libinstaller

TARGETS = syslinux.com

all: $(TARGETS)

tidy:
	-rm -f *.o *.i *.s *.a .*.d *.elf

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

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
%.o: %.S
	$(CC) -Wp,-MT,$@,-MD,.$@.d $(CFLAGS) -D__ASSEMBLY__ -c -o $@ $<
%.s: %.S
	$(CC) $(CFLAGS) -E -o $@ $<

-include .*.d
