#
# ONLY INCLUDE MIT OR 2/3-BSD-LICENSED CODE IN THIS LIBRARY
#

# Include configuration rules
NOGPL := 1
topdir = ../../
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/lib.mk

## OPTIONAL OBJECTS, AVAILABLE AS DYNAMIC LINKED MODULES
# PNG library object files
LIBPNG_OBJS = \
	libpng/png.o libpng/pngset.o libpng/pngget.o libpng/pngrutil.o  \
	libpng/pngtrans.o libpng/pngwutil.o libpng/pngread.o		\
	libpng/pngrio.o libpng/pngwio.o libpng/pngwrite.o		\
	libpng/pngrtran.o libpng/pngwtran.o libpng/pngmem.o		\
	libpng/pngerror.o libpng/pngpread.o

# JPG library object files
LIBJPG_OBJS = \
	jpeg/tinyjpeg.o jpeg/jidctflt.o	jpeg/decode1.o jpeg/decode3.o   \
	jpeg/rgb24.o jpeg/bgr24.o jpeg/yuv420p.o jpeg/grey.o		\
	jpeg/rgba32.o jpeg/bgra32.o					

LIBVESA_OBJS = \
	sys/vesacon_write.o sys/vesaserial_write.o			\
	sys/vesa/initvesa.o sys/vesa/drawtxt.o	sys/vesa/background.o	\
	sys/vesa/alphatbl.o sys/vesa/screencpy.o sys/vesa/fmtpixel.o	\
	sys/vesa/i915resolution.o

LIBMISC_OBJS = \
	sys/libansi.o sys/gpxe.o

LIBPCI_OBJS = \
	pci/cfgtype.o pci/scan.o pci/bios.o				\
	pci/readb.o pci/readw.o pci/readl.o				\
	pci/writeb.o pci/writew.o pci/writel.o

LIBSYSLINUX_OBJS = \
	syslinux/reboot.o syslinux/keyboard.o				\
	syslinux/features.o syslinux/config.o	\
	syslinux/dsinfo.o syslinux/version.o	\
	syslinux/pxe_get_cached.o syslinux/pxe_get_nic.o		\
	syslinux/pxe_dns.o						\
	syslinux/video/fontquery.o syslinux/video/forcetext.o		\
	syslinux/video/reportmode.o

DYNENTRY_OBJS = \
	atexit.o onexit.o abort.o

DYNLIBOBJS = \
	$(LIBZLIB_OBJS) \
	$(LIBPNG_OBJS) \
	$(LIBJPG_OBJS) \
	$(LIBPCI_OBJS) \
	$(LIBVESA_OBJS) \
	$(LIBSYSLINUX_OBJS) \
	$(LIBLOAD_OBJS) \
	$(LIBMISC_OBJS) \
	$(DYNENTRY_OBJS)


LIBOBJS = \
	$(DYNLIBOBJS)

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
DATADIR  = /usr/share
AUXDIR   = $(DATADIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libcom32.c32 libcom32min.a

libcom32.c32 : $(LIBOBJS)
	rm -f $@
	$(LD) -shared $(LDFLAGS) -o $@ $^

libcom32min.a : $(MINLIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy dist clean:
	rm -f sys/vesa/alphatbl.c
	find . \( -name \*.o -o -name \*.a -o -name .\*.d -o -name \*.tmp \) -print0 | \
		xargs -0r rm -f

spotless: clean
	rm -f *.a
	rm -f *~ \#* */*~ */\#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 com32.ld $(INSTALLROOT)$(COM32DIR)
	-rm -rf $(INSTALLROOT)$(COM32DIR)/include
	cp -r ../include $(INSTALLROOT)$(COM32DIR)

# These files are performance critical, and doesn't compile well with -Os
sys/vesa/drawtxt.o: sys/vesa/drawtxt.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -O3 -c -o $@ $<

sys/vesa/alphatbl.c: sys/vesa/alphatbl.pl
	$(PERL) $< > $@

jpeg/jidctflt.o: jpeg/jidctflt.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -O3 -c -o $@ $<

-include .*.d */.*.d */*/.*.d
