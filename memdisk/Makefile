#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Bostom MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

CC      = gcc
CFLAGS  = -g -O2 -fomit-frame-pointer -march=i386 -malign-functions=0 -malign-jumps=0 -malign-loops=0
LDFLAGS = 

all: e820func.o16 msetup.o16 e820test

clean:
	rm -f *.o *.s *.o16 *.s16 e820test

%.o16: %.s16
	as -o $@ $<

%.s16: %.s
	echo '.code16' | cat - $< > $@

%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

e820test: e820func.o msetup.o e820test.o
	$(CC) $(LDFLAGS) -o $@ $^

