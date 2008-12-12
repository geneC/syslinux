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
## Linux ext2/ext3 installer
##

topdir = ..
include $(topdir)/MCONFIG

OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libinstaller
CFLAGS	 = -W -Wall -Wno-sign-compare -D_FILE_OFFSET_BITS=64 \
	   $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = # -s

SRCS     = main.c \
	   ../libinstaller/setadv.c \
	   ../libinstaller/extlinux_bss_bin.c \
	   ../libinstaller/extlinux_sys_bin.c
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:../libinstaller

all: installer

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp

clean: tidy
	-rm -f extlinux

spotless: clean
	-rm -f *~

installer: extlinux

extlinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -Wp,-MT,$@,-MMD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

-include .*.d *.tmp
