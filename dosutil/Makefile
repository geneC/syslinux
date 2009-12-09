#
# OpenWatcom compile and link utility
#
WCL	= wcl
WCLOPT	= -6 -osx -mt -bt=DOS -l=COM

UPX     = upx

NASM    = nasm
NASMOPT = -O9999

TARGETS = mdiskchk.com eltorito.sys

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
	-rm -f *.com *.sys *~


