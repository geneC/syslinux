#
# ONLY INCLUDE MIT OR 2/3-BSD-LICENSED CODE IN THIS LIBRARY
#

# Include configuration rules
NOGPL := 1
topdir = ../../
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/lib.mk

LIBOBJS = \
	abort.o atexit.o atoi.o atol.o atoll.o calloc.o creat.o		\
	ctypes.o errno.o fgetc.o fgets.o fopen.o fprintf.o fputc.o	\
	fclose.o putchar.o setjmp.o					\
	fputs.o fread2.o fread.o free.o fwrite2.o fwrite.o 		\
	getopt.o getopt_long.o						\
	lrand48.o malloc.o stack.o memccpy.o memchr.o memcmp.o		\
	memcpy.o mempcpy.o memmem.o memmove.o memset.o memswap.o	\
	exit.o onexit.o	\
	perror.o printf.o puts.o qsort.o realloc.o seed48.o snprintf.o	\
	sprintf.o srand48.o sscanf.o stack.o strcasecmp.o strcat.o	\
	strchr.o strcmp.o strcpy.o strdup.o strlen.o			\
	strerror.o strnlen.o						\
	strncasecmp.o strncat.o strncmp.o strncpy.o strndup.o		\
	stpcpy.o stpncpy.o 						\
	strntoimax.o strntoumax.o strrchr.o strsep.o strspn.o strstr.o	\
	strtoimax.o strtok.o strtol.o strtoll.o strtoul.o strtoull.o	\
	strtoumax.o vfprintf.o vprintf.o vsnprintf.o vsprintf.o		\
	asprintf.o vasprintf.o strlcpy.o strlcat.o			\
	vsscanf.o zalloc.o						\
	skipspace.o							\
	chrreplace.o							\
	bufprintf.o							\
	inet.o								\
	\
	lmalloc.o lstrdup.o						\
	\
	dprintf.o vdprintf.o						\
	\
	suffix_number.o							\
	\
	sys/readdir.o getcwd.o chdir.o fdopendir.o			\
	\
	libgcc/__ashldi3.o libgcc/__udivdi3.o				\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o				\
	\
	sys/intcall.o sys/farcall.o sys/cfarcall.o sys/zeroregs.o	\
	sys/entry.o sys/exit.o sys/argv.o sys/times.o sys/sleep.o	\
	sys/fileinfo.o sys/opendev.o sys/read.o sys/write.o sys/ftell.o \
	sys/close.o sys/open.o sys/fileread.o sys/fileclose.o		\
	sys/openmem.o							\
	sys/isatty.o sys/fstat.o					\
	\
	sys/zfile.o sys/zfopen.o					\
	\
	sys/openconsole.o sys/line_input.o				\
	sys/colortable.o sys/screensize.o				\
	\
	sys/stdcon_read.o sys/stdcon_write.o sys/rawcon_read.o		\
	sys/rawcon_write.o sys/err_read.o sys/err_write.o		\
	sys/null_read.o sys/null_write.o sys/serial_write.o		\
	\
	sys/xserial_write.o						\
	\
	sys/ansi.o							\
	\
	sys/libansi.o							\
	\
	sys/gpxe.o							\
	\
	sys/ansicon_write.o sys/ansiserial_write.o			\
	\
	sys/vesacon_write.o sys/vesaserial_write.o			\
	sys/vesa/initvesa.o sys/vesa/drawtxt.o	sys/vesa/background.o	\
	sys/vesa/alphatbl.o sys/vesa/screencpy.o sys/vesa/fmtpixel.o	\
	sys/vesa/i915resolution.o					\
	\
	pci/cfgtype.o pci/scan.o pci/bios.o				\
	pci/readb.o pci/readw.o pci/readl.o				\
	pci/writeb.o pci/writew.o pci/writel.o				\
	\
	zlib/adler32.o zlib/compress.o zlib/crc32.o 			\
	zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o		\
	zlib/inflate.o zlib/infback.o zlib/inftrees.o zlib/inffast.o	\
	\
	libpng/png.o libpng/pngset.o libpng/pngget.o libpng/pngrutil.o  \
	libpng/pngtrans.o libpng/pngwutil.o libpng/pngread.o		\
	libpng/pngrio.o libpng/pngwio.o libpng/pngwrite.o		\
	libpng/pngrtran.o libpng/pngwtran.o libpng/pngmem.o		\
	libpng/pngerror.o libpng/pngpread.o				\
	\
	jpeg/tinyjpeg.o jpeg/jidctflt.o	jpeg/decode1.o jpeg/decode3.o   \
	jpeg/grey.o jpeg/yuv420p.o					\
	jpeg/rgb24.o jpeg/bgr24.o					\
	jpeg/rgba32.o jpeg/bgra32.o					\
	\
	sys/x86_init_fpu.o math/pow.o math/strtod.o			\
	\
	syslinux/idle.o	syslinux/reboot.o				\
	syslinux/features.o syslinux/config.o syslinux/serial.o		\
	syslinux/ipappend.o syslinux/dsinfo.o syslinux/version.o	\
	syslinux/keyboard.o						\
	\
	syslinux/memscan.o						\
	\
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
	syslinux/initramfs_archive.o					\
	\
	syslinux/pxe_get_cached.o syslinux/pxe_get_nic.o		\
	syslinux/pxe_dns.o						\
	\
	syslinux/adv.o syslinux/advwrite.o syslinux/getadv.o		\
	syslinux/setadv.o						\
	\
	syslinux/video/fontquery.o syslinux/video/forcetext.o		\
	syslinux/video/reportmode.o

# These are the objects which are also imported into the core
LIBCOREOBJS = 	\
	memcpy.o mempcpy.o memset.o memcmp.o memmove.o			\
	strlen.o stpcpy.o strcpy.o strcmp.o strlcpy.o strlcat.o		\
	strchr.o strncmp.o strncpy.o					\
	\
	snprintf.o sprintf.o vsnprintf.o				\
	\
	dprintf.o vdprintf.o						\
	\
	zalloc.o strdup.o						\
	\
	sys/intcall.o sys/farcall.o sys/cfarcall.o sys/zeroregs.o	\
	\
	libgcc/__ashldi3.o libgcc/__udivdi3.o				\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
DATADIR  = /usr/share
AUXDIR   = $(DATADIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libcom32.a libcomcore.a

libcom32.a : $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

libcomcore.a : $(LIBCOREOBJS)
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
	install -m 644 libcom32.a com32.ld $(INSTALLROOT)$(COM32DIR)
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
