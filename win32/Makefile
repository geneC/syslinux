## -----------------------------------------------------------------------
##
##   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
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
## Compiling on Cygwin
WINPREFIX  :=
WINCFLAGS  := -mno-cygwin -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
WINLDFLAGS := -mno-cygwin -Os -s
else
## Compiling on some variant of MinGW
ifeq ($(findstring MINGW32,$(OSTYPE)),MINGW32)
WINPREFIX  :=
else
WINPREFIX  := $(shell ./find-mingw.sh gcc)
endif
WINCFLAGS  := -W -Wall -Wno-sign-compare -Os -fomit-frame-pointer \
	      -D_FILE_OFFSET_BITS=64
WINLDFLAGS := -Os -s
endif
WINCFLAGS += -I. -I.. -I../libfat -I../libinstaller

WINCC      := $(WINPREFIX)gcc
WINAR	   := $(WINPREFIX)ar
WINRANLIB  := $(WINPREFIX)ranlib

WINCC_IS_GOOD := $(shell $(WINCC) $(WINCFLAGS) $(WINLDFLAGS) -o hello.exe hello.c >/dev/null 2>&1 ; echo $$?)

.SUFFIXES: .c .o .i .s .S

SRCS     = syslinux.c \
	   ../libinstaller/syslxmod.c \
	   ../libinstaller/bootsect_bin.c \
	   ../libinstaller/ldlinux_bin.c \
	   ../libinstaller/mbr_bin.c \
	   $(wildcard ../libfat/*.c)
OBJS     = $(patsubst %.c,%.o,$(notdir $(SRCS)))

VPATH = .:../libfat:../libinstaller

TARGETS = syslinux.exe

ifeq ($(WINCC_IS_GOOD),0)
all: $(TARGETS)
else
all:
	rm -f $(TARGETS)
endif

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp *_bin.c hello.exe

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

syslinux.exe: $(OBJS)
	$(WINCC) $(WINLDFLAGS) -o $@ $^


%.o: %.c
	$(WINCC) $(UMAKEDEPS) $(WINCFLAGS) -c -o $@ $<
%.i: %.c
	$(WINCC) $(UMAKEDEPS) $(WINCFLAGS) -E -o $@ $<
%.s: %.c
	$(WINCC) $(UMAKEDEPS) $(WINCFLAGS) -S -o $@ $<

-include .*.d *.tmp
