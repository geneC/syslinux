#
# LGPL/GPL code library
#

# Include configuration rules
topdir = ../..
include ../lib/MCONFIG

REQFLAGS += -I../gplinclude

LIBOBJS  = dmi/dmi_battery.o dmi/dmi_chassis.o dmi/dmi_memory.o            \
           dmi/dmi_processor.o dmi/dmi.o dmi/dmi_bios.o dmi/dmi_base_board.o \
	   dmi/dmi_ipmi.o cpuid.o vpd/vpd.o

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
