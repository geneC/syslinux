## $Id$
## -----------------------------------------------------------------------
##   
##   Copyright 1998-2004 H. Peter Anvin - All Rights Reserved
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
CC       = gcc
AR	 = ar
RANLIB   = ranlib
CFLAGS   = -mno-cygwin -W -Wall -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
PIC	 =
LDFLAGS  = -mno-cygwin -Os -s
else
ifeq ($(findstring MINGW32,$(OSTYPE)),MINGW32)
CC       = gcc
AR	 = ar
RANLIB   = ranlib
else
CC       = mingw-gcc
AR	 = mingw-ar
RANLIB   = mingw-ranlib
endif

CFLAGS   = -W -Wall -Wno-sign-compare -Os -fomit-frame-pointer -D_FILE_OFFSET_BITS=64
PIC      =
LDFLAGS  = -Os -s
endif
CFLAGS += -I. -I.. -I../libfat

CC_IS_GOOD := $(shell $(CC) $(CFLAGS) $(LDFLAGS) -o hello.exe hello.c >/dev/null 2>&1 ; echo $$?)

.SUFFIXES: .c .o .i .s .S

SRCS     = syslinux.c ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c $(wildcard ../libfat/*.c)
OBJS     = $(patsubst %.c,%.o,$(notdir $(SRCS)))

VPATH = .:..:../libfat

TARGETS = syslinux.exe

ifeq ($(CC_IS_GOOD),0)
all: $(TARGETS)
else
all:
	rm -f $(TARGETS)
endif

tidy:
	-rm -f *.o *.i *.s *.a .*.d

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

syslinux.exe: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -Wp,-MT,$@,-MMD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

-include .*.d
