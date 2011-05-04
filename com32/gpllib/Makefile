#
# LGPL/GPL code library
#

# Include configuration rules
topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/lib.mk

REQFLAGS += -I../gplinclude -I../gplinclude/zzjson

GPLDIRS := . disk dmi vpd acpi zzjson
LIBOBJS := $(foreach dir,$(GPLDIRS),$(patsubst %.c,%.o,$(wildcard $(dir)/*.c)))

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
DATADIR  = /usr/share
AUXDIR   = $(DATADIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libcom32gpl.a

libcom32gpl.a : $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy dist clean:
	find . \( -name \*.o -o -name \*.a -o -name .\*.d -o -name \*.tmp \) -print0 | \
		xargs -0r rm -f

spotless: clean
	rm -f *.a
	rm -f *~ \#* */*~ */\#*

# Mixing in the GPL include files is suboptimal, but I'm not sure
# there is a better way to do it.
install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libcom32gpl.a $(INSTALLROOT)$(COM32DIR)
	mkdir -p $(INSTALLROOT)$(COM32DIR)/include/
	cp -r ../gplinclude $(INSTALLROOT)$(COM32DIR)/include/

-include .*.d */.*.d */*/.*.d
