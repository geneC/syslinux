## -----------------------------------------------------------------------
##
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
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

TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)
CC	= gcc

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) -c -x c /dev/null -o $$tmpf 2>/dev/null; \
	           then echo $(1); else echo $(2); fi; rm -f $$tmpf)

M32     := $(call gcc_ok,-m32,) $(call gcc_ok,-fno-stack-protector,)

LD         = ld -m elf_i386
AR	   = ar
NASM	   = nasm
NASMOPT	   = -O9999
RANLIB	   = ranlib
CFLAGS     = $(M32) -mregparm=3 -DREGPARM=3 -W -Wall -march=i386 -Os \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I./include -I../include \
	     -Wp,-MT,$@,-MD,$(dir $@).$(notdir $@).d
SFLAGS     = $(M32) -D__COM32__ -march=i386
LDFLAGS    = -T ../lib/com32.ld
LNXCFLAGS  = -I./include -W -Wall -O -g -D_GNU_SOURCE
LNXSFLAGS  = -g
LNXLDFLAGS = -g
OBJCOPY    = objcopy
LIBOBJS	   = ansiline.o ansiraw.o get_key.o sha1hash.o unbase64.o \
	     md5.o crypt-md5.o sha256crypt.o sha512crypt.o base64.o
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

tidy dist:
	rm -f *.o *.lo *.lst *.elf .*.d

clean: tidy
	rm -f *.lss *.a *.c32 *.lnx *.com

spotless: clean
	rm -f *~ \#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libutil_com.a libutil_lnx.a $(INSTALLROOT)$(COM32DIR)

-include .*.d
