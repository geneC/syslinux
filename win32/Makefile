## -----------------------------------------------------------------------
##
##   Copyright 1998-2006 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# Makefile for SYSLINUX Win32
#
# This is separated out mostly so we can have a different set of Makefile
# variables.
#

OSTYPE   = $(shell uname -msr)
ifeq ($(findstring CYGWIN,$(OSTYPE)),CYGWIN)
WINCC       = gcc
WINAR	    = ar
WINRANLIB   = ranlib
WINCFLAGS   = -mno-cygwin -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
WINPIC	    =
WINLDFLAGS  = -mno-cygwin -Os -s
else
ifeq ($(findstring MINGW32,$(OSTYPE)),MINGW32)
WINCC       = gcc
WINAR	    = ar
WINRANLIB   = ranlib
else
WINCC       = i386-mingw32-gcc
WINAR	    = i386-mingw32-ar
WINRANLIB   = i386-mingw32-ranlib
endif

WINCFLAGS   = -W -Wall -Wno-sign-compare -Os -fomit-frame-pointer \
	-D_FILE_OFFSET_BITS=64
WINPIC      =
WINLDFLAGS  = -Os -s
endif
WINCFLAGS += -I. -I.. -I../libfat -I../libinstaller

WINCC_IS_GOOD := $(shell $(WINCC) $(WINCFLAGS) $(WINLDFLAGS) -o hello.exe hello.c >/dev/null 2>&1 ; echo $$?)

.SUFFIXES: .c .o .i .s .S

SRCS     = syslinux.c ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c \
	   ../mbr_bin.c $(wildcard ../libfat/*.c)
OBJS     = $(patsubst %.c,%.o,$(notdir $(SRCS)))

VPATH = .:..:../libfat:../libinstaller

TARGETS = syslinux.exe

ifeq ($(WINCC_IS_GOOD),0)
all: $(TARGETS)
else
all:
	rm -f $(TARGETS)
endif

tidy:
	-rm -f *.o *.i *.s *.a .*.d *_bin.c

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

syslinux.exe: $(OBJS)
	$(WINCC) $(WINLDFLAGS) -o $@ $^


%.o: %.c
	$(WINCC) -Wp,-MT,$@,-MMD,.$@.d $(WINCFLAGS) -c -o $@ $<
%.i: %.c
	$(WINCC) $(WINCFLAGS) -E -o $@ $<
%.s: %.c
	$(WINCC) $(WINCFLAGS) -S -o $@ $<

-include .*.d
