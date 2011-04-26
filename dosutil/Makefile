#
# OpenWatcom compile and link utility
#
topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/syslinux.mk

WCL	= wcl
WCLOPT	= -6 -osx -mt -bt=DOS -l=COM

UPX     = upx

NASM    = nasm
NASMOPT = -O9999

WCTARGETS = mdiskchk.com
NSTARGETS = eltorito.sys copybs.com
TARGETS   = $(WCTARGETS) $(NSTARGETS)

%.obj: %.c
	$(WCL) $(WCLOPT) -c -fo=$@ $<

%.com: %.obj
	$(WCL) $(WCLOPT) -fe=$@ $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true
	rm -f $*.0*
	chmod a-x $@

%.sys: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true
	rm -f $*.0*
	chmod a-x $@

%.com: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true
	rm -f $*.0*
	chmod a-x $@

all: $(TARGETS)

tidy dist:
	-rm -f *.obj *.lst *.o *.0*

clean: tidy

spotless: clean
	-rm -f $(NSTARGETS) *~

installer: all

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)/dosutil
	install -m 644 $(TARGETS) $(INSTALLROOT)$(AUXDIR)/dosutil
