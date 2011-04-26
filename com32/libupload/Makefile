# Include configuration rules
topdir = ../..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/com32.mk

REQFLAGS += -I./

SUBDIRS := . 
LIBOBJS := $(foreach dir,$(SUBDIRS),$(patsubst %.c,%.o,$(wildcard $(dir)/*.c)))

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
DATADIR  = /usr/share
AUXDIR   = $(DATADIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libcom32upload.a

libcom32upload.a : $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy dist clean:
	find . \( -name \*.o -o -name \*.a -o -name .\*.d -o -name \*.tmp \) -print0 | \
		xargs -0r rm -f

spotless: clean
	rm -f *.a
	rm -f *~ \#* */*~ */\#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libcom32upload.a $(INSTALLROOT)$(COM32DIR)
	mkdir -p $(INSTALLROOT)$(COM32DIR)/include/
	cp -r *.h $(INSTALLROOT)$(COM32DIR)/include/

-include .*.d */.*.d */*/.*.d
