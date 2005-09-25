#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001-2004 H. Peter Anvin - All Rights Reserved
##
##   Permission is hereby granted, free of charge, to any person
##   obtaining a copy of this software and associated documentation
##   files (the "Software"), to deal in the Software without
##   restriction, including without limitation the rights to use,
##   copy, modify, merge, publish, distribute, sublicense, and/or
##   sell copies of the Software, and to permit persons to whom
##   the Software is furnished to do so, subject to the following
##   conditions:
##   
##   The above copyright notice and this permission notice shall
##   be included in all copies or substantial portions of the Software.
##   
##   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
##   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
##   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
##   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
##   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
##   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
##   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
##   OTHER DEALINGS IN THE SOFTWARE.
##
## -----------------------------------------------------------------------

##
## Utility companion library for the COM32 library
##

gcc_ok   = $(shell if gcc $(1) -c -x c /dev/null -o /dev/null 2>/dev/null; \
	           then echo $(1); else echo $(2); fi)

M32     := $(call gcc_ok,-m32,)

CC         = gcc
LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
RANLIB	   = ranlib
CFLAGS     = $(M32) -mregparm=3 -DREGPARM=3 -D__COM32__ -W -Wall -march=i386 -Os -fomit-frame-pointer -I./include -I../include
SFLAGS     = $(M32) -D__COM32__ -march=i386
LDFLAGS    = -T ../lib/com32.ld
LNXCFLAGS  = -I./include -W -Wall -O -g
LNXSFLAGS  = -g
LNXLDFLAGS = -g
OBJCOPY    = objcopy
LIBOBJS	   = ansiline.o ansiraw.o get_key.o idle.o sha1hash.o unbase64.o \
	     movebits.o loadfile.o
LNXLIBOBJS = $(patsubst %.o,%.lo,$(LIBOBJS))

.SUFFIXES: .lss .c .lo .o .elf .c32 .lnx

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
AUXDIR   = $(LIBDIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libutil_com.a libutil_lnx.a

libutil_com.a: $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $(LIBOBJS)
	$(RANLIB) $@

libutil_lnx.a: $(LNXLIBOBJS)
	rm -f $@
	$(AR) cq $@ $(LNXLIBOBJS)
	$(RANLIB) $@

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PRECIOUS: %.elf
%.elf: %.o $(LIB)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBGCC)

.PRECIOUS: %.lo
%.lo: %.S
	$(CC) $(LNXSFLAGS) -c -o $@ $<

.PRECIOUS: %.lo
%.lo: %.c
	$(CC) $(LNXCFLAGS) -c -o $@ $<

.PRECIOUS: %.lnx
%.lnx: %.lo
	$(CC) $(LNXCFLAGS) -o $@ $^

%.c32: %.elf
	$(OBJCOPY) -O binary $< $@

tidy:
	rm -f *.o *.lo *.lst *.elf

clean: tidy
	rm -f *.lss *.a *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libutil_com.a libutil_lnx.a $(INSTALLROOT)$(COM32DIR)

