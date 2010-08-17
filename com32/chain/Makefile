## -----------------------------------------------------------------------
##
##   Copyright 2001-2010 H. Peter Anvin - All Rights Reserved
##   Copyright 2010 Michal Soltys
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------


topdir = ../..
include ../MCONFIG

OBJS = chain.o partiter.o
GCCEXTRA = -Wextra -Wconversion -pedantic -Wno-error -DDEBUG

all: chain.c32

chain.elf: $(OBJS) $(LIBS) $(C_LIBS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) $(GCCEXTRA) -c -o $@ $<

tidy dist:
	rm -f *.o *.lo *.a *.lst *.elf .*.d *.tmp

clean: tidy
	rm -f *.lnx

spotless: clean
	rm -f *.lss *.c32 *.com
	rm -f *~ \#*

install:


-include .*.d
