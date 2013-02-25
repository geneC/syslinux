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

# ZIP library object files
LIBZLIB_OBJS = \
	zlib/adler32.o zlib/compress.o zlib/crc32.o 			\
	zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o		\
	zlib/inflate.o zlib/infback.o zlib/inftrees.o zlib/inffast.o	\
	sys/zfile.o sys/zfopen.o

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
	syslinux/version.o						\
	syslinux/pxe_get_cached.o syslinux/pxe_get_nic.o		\
	syslinux/pxe_dns.o						\
	syslinux/video/fontquery.o syslinux/video/reportmode.o

LIBLOAD_OBJS = \
	syslinux/addlist.o syslinux/freelist.o syslinux/memmap.o	\
	syslinux/movebits.o syslinux/shuffle.o syslinux/shuffle_pm.o	\
	syslinux/shuffle_rm.o syslinux/zonelist.o			\
	syslinux/dump_mmap.o syslinux/dump_movelist.o			\
	\
	syslinux/run_default.o syslinux/run_command.o			\
	syslinux/cleanup.o syslinux/localboot.o	syslinux/runimage.o	\
	\
	syslinux/loadfile.o syslinux/floadfile.o syslinux/zloadfile.o	\
	\
	syslinux/load_linux.o syslinux/initramfs.o			\
	syslinux/initramfs_file.o syslinux/initramfs_loadfile.o		\
	syslinux/initramfs_archive.o

DYNENTRY_OBJS = \
	atexit.o onexit.o abort.o

## CORE OBJECTS, INCLUDED IN THE ROOT COM32 MODULE
LIBENTRY_OBJS = \
	sys/intcall.o sys/farcall.o sys/cfarcall.o sys/zeroregs.o	\
	sys/argv.o sys/sleep.o						\
	sys/fileinfo.o sys/opendev.o sys/read.o sys/write.o sys/ftell.o \
	sys/close.o sys/open.o sys/fileread.o sys/fileclose.o		\
	sys/openmem.o					\
	sys/isatty.o sys/fstat.o					\
	\
	dprintf.o vdprintf.o						\
	\
	syslinux/idle.o							\
	\
	exit.o

LIBMODULE_OBJS = \
	sys/module/common.o sys/module/elf_module.o		\
	sys/module/elfutils.o					\
	sys/module/exec.o

LIBGCC_OBJS = \
	libgcc/__ashldi3.o libgcc/__udivdi3.o			\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o

LIBCONSOLE_OBJS = \
	\
	sys/openconsole.o sys/line_input.o				\
	sys/colortable.o sys/screensize.o				\
	\
	sys/stdcon_read.o sys/rawcon_read.o		\
	sys/rawcon_write.o						\
	sys/null_write.o sys/serial_write.o		\
	\
	sys/xserial_write.o						\
	\
	sys/ansi.o							\
	\
	sys/ansicon_write.o sys/ansiserial_write.o	\
	\
	syslinux/serial.o

LIBOTHER_OBJS = \
	atoi.o atol.o atoll.o calloc.o creat.o		\
	fgets.o fprintf.o fputc.o	\
	putchar.o				\
	getopt.o getopt_long.o						\
	lrand48.o stack.o memccpy.o memchr.o 		\
	mempcpy.o memmem.o memmove.o memswap.o	\
	perror.o qsort.o seed48.o \
	srand48.o sscanf.o strcasecmp.o 				\
	strerror.o errlist.o		\
	strnlen.o							\
	strncat.o strndup.o		\
	stpncpy.o						\
	strntoimax.o strsep.o strspn.o strstr.o				\
	strtoimax.o strtok.o strtol.o strtoll.o strtoull.o		\
	strtoumax.o vprintf.o vsprintf.o		\
	asprintf.o vasprintf.o			\
	vsscanf.o							\
	skipspace.o							\
	chrreplace.o							\
	bufprintf.o							\
	inet.o dhcppack.o dhcpunpack.o					\
	strreplace.o							\
	lstrdup.o						\
	\
	suffix_number.o							\
	\
	getcwd.o fdopendir.o	\
	\
	sys/line_input.o				\
	sys/colortable.o sys/screensize.o				\
	\
	sys/stdcon_read.o sys/stdcon_write.o sys/rawcon_read.o		\
	sys/rawcon_write.o		\
	sys/null_read.o sys/null_write.o sys/serial_write.o		\
	\
	sys/xserial_write.o						\
	\
	sys/ansi.o							\
	\
	sys/ansicon_write.o sys/ansiserial_write.o			\
	\
	pci/cfgtype.o pci/scan.o pci/bios.o					\
	pci/readb.o pci/readw.o pci/readl.o			\
	pci/writeb.o pci/writew.o pci/writel.o	\
	\
	sys/x86_init_fpu.o math/pow.o math/strtod.o			\
	syslinux/disk.o							\
	\
	syslinux/setup_data.o

CORELIBOBJS = \
	memcpy.o memset.o memcmp.o printf.o strncmp.o vfprintf.o 	\
	strlen.o vsnprintf.o snprintf.o stpcpy.o strcmp.o strdup.o 	\
	strcpy.o strncpy.o setjmp.o fopen.o fread.o fread2.o puts.o 	\
	sprintf.o strlcat.o strchr.o strlcpy.o strncasecmp.o ctypes.o 	\
	fputs.o fwrite2.o fwrite.o fgetc.o fclose.o lmalloc.o strtoul.o	\
	sys/err_read.o sys/err_write.o sys/null_read.o strntoumax.o	\
	sys/stdcon_write.o						\
	syslinux/memscan.o strrchr.o strcat.o				\
	libgcc/__ashldi3.o libgcc/__udivdi3.o				\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o				\
	$(LIBENTRY_OBJS) \
	$(LIBMODULE_OBJS)

MINLIBOBJS = \
	syslinux/ipappend.o \
	syslinux/dsinfo.o \
	$(LIBOTHER_OBJS) \
	$(LIBGCC_OBJS) \
	$(LIBCONSOLE_OBJS) \
	$(LIBLOAD_OBJS) \
	$(LIBZLIB_OBJS)
#	$(LIBVESA_OBJS)


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

all: libcom32.c32 libcom32min.a libcom32core.a

libcom32.elf : $(LIBOBJS)
	rm -f $@
	$(LD) -shared $(LDFLAGS) -soname $(patsubst %.elf,%.c32,$(@F)) -o $@ $^

libcom32min.a : $(MINLIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

libcom32core.a : $(CORELIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy dist clean:
	rm -f sys/vesa/alphatbl.c errlist.c
	find . \( -name \*.o -o -name \*.a -o -name .\*.d -o -name \*.tmp \) -print0 | \
		xargs -0r rm -f

spotless: clean
	rm -f *.a *.c32
	rm -f *~ \#* */*~ */\#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 com32.ld $(INSTALLROOT)$(COM32DIR)
	-rm -rf $(INSTALLROOT)$(COM32DIR)/include
	cp -r ../include $(INSTALLROOT)$(COM32DIR)

errlist.c: makeerrlist.pl ../include/errno.h
	$(PERL) $<  $(CFLAGS) -errlist > $@ || rm -f $@

# These files are performance critical, and doesn't compile well with -Os
sys/vesa/drawtxt.o: sys/vesa/drawtxt.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -O3 -c -o $@ $<

sys/vesa/alphatbl.c: sys/vesa/alphatbl.pl
	$(PERL) $< > $@

jpeg/jidctflt.o: jpeg/jidctflt.c
	$(CC) $(MAKEDEPS) $(CFLAGS) -O3 -c -o $@ $<

-include .*.d */.*.d */*/.*.d
