#
# OpenWatcom compile and link utility
#
WCL	= wcl
WCLOPT	= -6 -osx -mt -bt=DOS -l=COM

UPX     = upx

TARGETS = mdiskchk.com

%.obj: %.c
	$(WCL) $(WCLOPT) -c -fo=$@ $<

%.com: %.obj
	$(WCL) $(WCLOPT) -fe=$@ $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true

all: $(TARGETS)

tidy dist:
	-rm -f *.obj *.o

clean: tidy

spotless: clean
	-rm -f *.com *.sys *~


