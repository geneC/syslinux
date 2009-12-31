#
# OpenWatcom compile and link utility
#
topdir = ..
include $(topdir)/MCONFIG

WCL	= wcl
WCLOPT	= -6 -osx -mt -bt=DOS -l=COM

UPX     = upx

NASM    = nasm
NASMOPT = -O9999

WCTARGETS = mdiskchk.com
NSTARGETS = eltorito.sys
TARGETS   = $(WCTARGETS) $(NSTARGETS)

%.obj: %.c
	$(WCL) $(WCLOPT) -c -fo=$@ $<

%.com: %.obj
	$(WCL) $(WCLOPT) -fe=$@ $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true

%.sys: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true

%.com: %.asm
	$(NASM) $(NASMOPT) -f bin -o $@ -l $*.lst $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true

all: $(TARGETS)

tidy dist:
	-rm -f *.obj *.lst *.o

clean: tidy

spotless: clean
	-rm -f $(NSTARGETS) *~

installer: all

install: installer
	mkdir -m 755 -p $(INSTALLROOT)$(AUXDIR)/dosutil
	install -m 644 $(TARGETS) $(INSTALLROOT)$(AUXDIR)/dosutil
