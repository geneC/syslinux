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
CFLAGS  = -g -O2 -fomit-frame-pointer -march=i386 \
	  -malign-functions=0 -malign-jumps=0 -malign-loops=0
LDFLAGS = 
AS      = as
LD	= ld
NASM    = nasm
OBJCOPY = objcopy
PERL    = perl

# Important: init.o16 must be first!!
OBJS    = init.o16 setup.o16 msetup.o16 e820func.o16 conio.o16 memdisk.o

all: memdisk e820test

clean:
	rm -f *.o *.s *.o16 *.s16 *.bin *.lst *.elf e820test memdisk

%.o16: %.s16
	$(AS) -o $@ $<

%.s16: %.s
	echo '.code16' | cat - $< > $@

%.s: %.S
	$(CC) -x c $(CFLAGS) -Wp,-traditional -E -o $@ $<

%.s16: %.S16
	$(CC) -x c $(CFLAGS) -Wp,-traditional -E -o $@ $<

%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.bin: %.asm
	$(NASM) -f bin -o $@ -l $*.lst $<

memdisk.elf: $(OBJS)
	$(LD) -Ttext 0 -o $@ $^

memdisk: memdisk.elf postprocess.pl
	$(OBJCOPY) -O binary $< $@
	$(PERL) postprocess.pl $@

e820test: e820func.o msetup.o e820test.o memdisk.o
	$(CC) $(LDFLAGS) -o $@ $^

memdisk.o: memdisk.bin
	$(LD) -r -b binary -o $@ $<
