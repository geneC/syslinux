## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## samples for syslinux users
##

topdir = ..
include $(topdir)/MCONFIG.embedded

INCLUDES   = -I$(com32)/include

PPMTOLSS16 = $(topdir)/utils/ppmtolss16
LIBGCC    := $(shell $(CC) $(GCCOPT) --print-libgcc)
LIB	   = liboldcom32.a

LIBOBJS	   = conio.o atou.o skipatou.o printf.o c32exit.o

com32 = $(topdir)/com32
C_LIBS	   = $(com32)/libutil/libutil_com.a $(com32)/lib/libcom32.a $(LIBGCC)
LDFLAGS    = -m elf_i386 -T $(com32)/lib/com32.ld

all: syslogo.lss comecho.com hello.c32 hello2.c32 filetest.c32 c32echo.c32 \
     fd.c32 $(LIB)

.PRECIOUS: %.o

.PRECIOUS: %.elf
%.elf: c32entry.o %.o $(LIB)
	$(LD) -Ttext 0x101000 -e _start -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

%.com: %.asm
	( $(NASM) -M -DDEPEND -o $@ $< ; echo '' ) > .$@.d ; true
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<

$(LIB): $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

%.lss:	%.ppm.gz $(PPMTOLSS16)
	$(GZIPPROG) -cd $< | \
		$(PPMTOLSS16) \#000000=0 \#d0d0d0=7 \#f6f6f6=15	\
		> $@

%.ppm.gz: %.png
	$(PNGTOPNM) $< | gzip -9 > $@

tidy dist:
	rm -f *.o *.a *.lst *.elf .*.d

# Don't specify *.com since mdiskchk.com can't be built using Linux tools
clean: tidy
	rm -f *.lss *.o *.c32 comecho.com

spotless: clean

-include .*.d
