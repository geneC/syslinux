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
## memory dump utility
##

topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/embedded.mk

OPTFLAGS = 
INCLUDES = -include code16.h -I.
LDFLAGS	 = -T com16.ld

SRCS     = main.c serial.c ymsend.c srecsend.c
OBJS	 = crt0.o $(patsubst %.c,%.o,$(notdir $(SRCS)))
LIBOBJS	 = conio.o memcpy.o memset.o skipatou.o strtoul.o \
	   argv.o printf.o __divdi3.o __udivmoddi4.o

.SUFFIXES: .c .o .i .s .S .elf .com

TARGETS = memdump.com

all: $(TARGETS)

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp *.elf

clean: tidy

spotless: clean
	-rm -f *~ $(TARGETS)

installer:

memdump.elf: $(OBJS) libcom.a
	$(LD) $(LDFLAGS) -o $@ $^

libcom.a: $(LIBOBJS)
	-rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

memdump.com: memdump.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -S -o $@ $<
%.o: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -c -o $@ $<
%.s: %.S
	$(CC) $(MAKEDEPS) $(SFLAGS) -E -o $@ $<

-include .*.d *.tmp
