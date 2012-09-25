## -----------------------------------------------------------------------
##
##   Copyright 1998-2009 H. Peter Anvin - All Rights Reserved
##   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for the SYSLINUX core
#

VPATH = $(SRC)

# No builtin rules
MAKEFLAGS += -r
MAKE      += -r

ifdef EFI_BUILD
include $(MAKEDIR)/lib.mk
include $(MAKEDIR)/efi.mk
else
include $(MAKEDIR)/embedded.mk
-include $(objdir)/version.mk
endif

OPTFLAGS =
INCLUDES = -I$(SRC)/include -I$(com32)/include -I$(com32)/include/sys -I$(com32)/lib

# This is very similar to cp437; technically it's for Norway and Denmark,
# but it's unlikely the characters that are different will be used in
# filenames by other users.
CODEPAGE = cp865

# The targets to build in this directory...
BTARGET  = kwdhash.gen \
	   ldlinux.bss ldlinux.sys ldlinux.bin \
	   isolinux.bin isolinux-debug.bin pxelinux.0

# All primary source files for the main syslinux files
NASMSRC	 := $(wildcard $(SRC)/*.asm)
NASMHDR  := $(wildcard $(SRC)/*.inc)
CSRC	 := $(wildcard $(SRC)/*.c $(SRC)/*/*.c $(SRC)/*/*/*.c)
SSRC	 := $(wildcard $(SRC)/*.S $(SRC)/*/*.S $(SRC)/*/*/*.S)
CHDR	 := $(wildcard $(SRC)/*.h $(SRC)/*/*.h $(SRC)/*/*/*.h)
OTHERSRC := keywords
ALLSRC    = $(NASMSRC) $(NASMHDR) $(CSRC) $(SSRC) $(CHDR) $(OTHERSRC)

COBJ	 := $(subst $(SRC)/,,$(patsubst %.c,%.o,$(CSRC)))
SOBJ	 := $(subst $(SRC)/,,$(patsubst %.S,%.o,$(SSRC)))

# Don't include derivative-specific objects
COBJS	 = $(filter-out %rawcon.o %plaincon.o %pxelinux-c.o %ldlinux-c.o %isolinux-c.o,$(COBJ))

LIB	 = libcom32.a
LIBS	 = $(LIB) --whole-archive $(objdir)/com32/lib/libcom32core.a
LIBDEP   = $(filter-out -% %start%,$(LIBS))
LIBOBJS  = $(COBJS) $(SOBJ)

NASMDEBUG = -g -F dwarf
NASMOPT  += $(NASMDEBUG)

PREPCORE = $(OBJ)/../lzo/prepcore

CFLAGS += -D__SYSLINUX_CORE__

# The DATE is set on the make command line when building binaries for
# official release.  Otherwise, substitute a hex string that is pretty much
# guaranteed to be unique to be unique from build to build.
ifndef HEXDATE
HEXDATE := $(shell $(PERL) $(SRC)/../now.pl $(SRCS))
endif
ifndef DATE
DATE    := $(shell sh $(SRC)/../gen-id.sh $(VERSION) $(HEXDATE))
endif

# Set up the NASM and LD options for the architecture
NASM_ELF = "unknown"
LD_PIE = "unknown"
ifeq ($(ARCH),i386)
	NASM_ELF = elf
	LD_PIE = -pie
endif
ifeq ($(ARCH),x86_64)
	NASM_ELF = elf64
	#LD_PIE = --pic-executable
	LD_PIE = 
endif

ifdef EFI_BUILD
all: makeoutputdirs $(filter-out %bios.o,$(COBJS)) codepage.o
else
all: makeoutputdirs $(BTARGET)
endif

makeoutputdirs:
	@mkdir -p $(sort $(dir $(COBJ) $(SOBJ)))

kwdhash.gen: keywords genhash.pl
	$(PERL) $(SRC)/genhash.pl < $(SRC)/keywords > $(OBJ)/kwdhash.gen

.PRECIOUS: %.elf

%.raw: %.elf
	$(OBJCOPY) -O binary -S $< $(@:.bin=.raw)

# GNU make 3.82 gets confused by the first form
.PRECIOUS: $(OBJ)/%.raw

%.bin: %.raw $(PREPCORE)
	$(PREPCORE) $< $@

%.o: %.asm kwdhash.gen $(OBJ)/../version.gen
	$(NASM) -f $(NASM_ELF) $(NASMOPT) -DDATE_STR="'$(DATE)'" \
		-DHEXDATE="$(HEXDATE)" \
		-D$(ARCH) \
		-I$(SRC)/ \
		-l $(@:.o=.lsr) -o $@ -MP -MD $(dir $@).$(notdir $@).d $<

AUXLIBS = libisolinux.a libisolinux-debug.a libldlinux.a libpxelinux.a
LDSCRIPT = $(SRC)/$(ARCH)/syslinux.ld

%.elf: %.o $(LIBDEP) $(LDSCRIPT) $(AUXLIBS)
	$(LD) $(LDFLAGS) -Bsymbolic $(LD_PIE) -E --hash-style=gnu -T $(LDSCRIPT) -M -o $@ $< \
		--start-group $(LIBS) $(subst $(*F).elf,lib$(*F).a,$@) --end-group \
		> $(@:.elf=.map)
	$(OBJDUMP) -h $@ > $(@:.elf=.sec)
	$(PERL) $(SRC)/lstadjust.pl $(@:.elf=.lsr) $(@:.elf=.sec) $(@:.elf=.lst)

libisolinux.a: rawcon.o isolinux-c.o
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

libisolinux-debug.a: libisolinux.a
	cp $^ $@

libpxelinux.a: rawcon.o pxelinux-c.o
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

libldlinux.a: plaincon.o ldlinux-c.o
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

$(LIB): $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

pxelinux.0: pxelinux.bin
	cp -f $< $@

ldlinux.bss: ldlinux.bin
	dd if=$< of=$@ bs=512 count=1

ldlinux.sys: ldlinux.bin
	dd if=$< of=$@ bs=512 skip=2

codepage.cp: $(OBJ)/../codepage/$(CODEPAGE).cp
	cp -f $< $@

codepage.o: codepage.S codepage.cp

install: installer

install-lib: installer

install-all: install install-lib

netinstall: installer

tidy dist:
	rm -f codepage.cp *.o *.elf *.a stupid.* patch.offset .depend .*.d
	rm -f *.elf.tmp *.sym
	rm -f *.lsr *.lst *.map *.sec *.raw
	rm -f */*.o */*/*.o */*.lst */*/*.lst */.*.d */*/.*.d
	rm -f $(OBSOLETE) $(LIB)

clean: tidy

spotless: clean
	rm -f $(BTARGET) *.bin *_bin.c

# Include dependencies file
-include .*.d */.*.d */*/.*.d
