## -*- makefile -*- ------------------------------------------------------
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

topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/build.mk

INCLUDES += -I./include

LIBOBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
LIB     = lzo.a
BINS    = prepcore

all : $(BINS)

$(LIB) : $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

prepcore : prepcore.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

tidy dist clean spotless:
	rm -f $(BINS)
	rm -f *.o *.a .*.d
	rm -f */*.o */*.a */.*.d

installer:

-include .*.d */.*.d
