#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001-2005 H. Peter Anvin - All Rights Reserved
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

gcc_ok   = $(shell if gcc $(1) -c -x c /dev/null -o /dev/null 2>/dev/null; \
	           then echo $(1); else echo $(2); fi)

M32     := $(call gcc_ok,-m32,)

CC         = gcc
LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
RANLIB	   = ranlib
COM32DIR   = ../com32
LUDIR      = $(COM32DIR)/libutil
LDIR       = $(COM32DIR)/lib
CFLAGS     = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -march=i386 -Os -fomit-frame-pointer -I$(LUDIR)/include -I$(COM32DIR)/include  -Ilibmenu -D__COM32__
SFLAGS     = -D__COM32__ -march=i386
LDFLAGS    = -T $(LDIR)/com32.ld 
OBJCOPY    = objcopy
LIBGCC    := $(shell $(CC) --print-libgcc)

LIBS	   = libmenu/libmenu.a $(LUDIR)/libutil_com.a $(LDIR)/libcom32.a $(LIBGCC)

LIBMENU = libmenu/syslnx.o libmenu/com32io.o libmenu/tui.o \
	libmenu/menu.o libmenu/passwords.o libmenu/des.o libmenu/help.o 

CMENUS = $(patsubst %.c,%.c32,$(wildcard *.c))
IMENUS = $(patsubst %.menu,%.c32,$(wildcard *.menu))

MENUS = $(CMENUS) $(IMENUS)

.SUFFIXES: .S .c .o .elf .c32 .menu

.PRECIOUS: %.c
%.c: %.menu
	python menugen.py $< $@

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf 
%.elf: %.o $(LIBS)
	$(LD) $(LDFLAGS) -o $@ $^ 

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

all: menus

libmenu/libmenu.a: $(LIBMENU)
	-rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy:
	rm -f *.o *.lo *.a *.lst *.elf libmenu/*.o libmenu/*.a

clean: tidy
	rm -f *.lss *.c32 *.com

spotless: clean
	rm -f *~ \#*

menus: $(MENUS)

install:	# Don't install samples
