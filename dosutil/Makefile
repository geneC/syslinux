#
# OpenWatcom compile and link utility
#
WCL           = wcl
WCLOPT_DOSCOM = -3 -osx -mt -bt=DOS -l=COM

UPX           = upx

%.com: %.c
	$(WCL) $(WCLOPT_DOSCOM) -fe=$@ $<
	$(UPX) --ultra-brute --lzma $@ || \
		$(UPX) --ultra-brute $@ || \
		true

all: mdiskchk.com

clean:
	rm -f *.o *.obj *.com *.sys
