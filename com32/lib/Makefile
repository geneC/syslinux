# Include configuration rules
include MCONFIG

LIBOBJS = \
	abort.o atexit.o atoi.o atol.o atoll.o calloc.o creat.o		\
	ctypes.o errno.o fgetc.o fgets.o fopen.o fprintf.o fputc.o	\
	putchar.o setjmp.o						\
	fputs.o fread2.o fread.o free.o fwrite2.o fwrite.o getopt.o	\
	lrand48.o malloc.o stack.o memccpy.o memchr.o memcmp.o		\
	memcpy.o memmem.o memmove.o memset.o memswap.o exit.o onexit.o	\
	perror.o printf.o puts.o qsort.o realloc.o seed48.o snprintf.o	\
	sprintf.o srand48.o sscanf.o stack.o strcasecmp.o strcat.o	\
	strchr.o strcmp.o strcpy.o strdup.o strerror.o strlen.o		\
	strncasecmp.o strncat.o strncmp.o strncpy.o strndup.o		\
	strntoimax.o strntoumax.o strrchr.o strsep.o strspn.o strstr.o	\
	strtoimax.o strtok.o strtol.o strtoll.o strtoul.o strtoull.o	\
	strtoumax.o vfprintf.o vprintf.o vsnprintf.o vsprintf.o		\
	asprintf.o vasprintf.o strlcpy.o strlcat.o 			\
	vsscanf.o							\
	\
	libgcc/__ashldi3.o libgcc/__udivdi3.o			\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o 				\
	\
	sys/intcall.o sys/farcall.o					\
	sys/entry.o sys/exit.o sys/argv.o sys/times.o sys/idle.o	\
	sys/fileinfo.o sys/opendev.o sys/read.o sys/write.o sys/ftell.o \
	sys/close.o sys/open.o sys/fileread.o sys/fileclose.o		\
	sys/isatty.o sys/fstat.o sys/openconsole.o sys/line_input.o	\
	sys/colortable.o						\
	\
	sys/stdcon_read.o sys/stdcon_write.o sys/rawcon_read.o		\
	sys/rawcon_write.o sys/err_read.o sys/err_write.o		\
	sys/null_read.o sys/null_write.o sys/serial_write.o		\
	\
	sys/xserial_write.o						\
	\
	sys/ansicon_write.o sys/ansiserial_write.o			\
	\
	sys/vesacon_write.o sys/vesaserial_write.o 			\
	sys/vesa/initvesa.o sys/vesa/drawtxt.o	sys/vesa/background.o	\
	\
	pci/cfgtype.o 							\
	pci/readb.o pci/readw.o pci/readl.o pci/readbios.o 		\
	pci/writeb.o pci/writew.o pci/writel.o pci/writebios.o 		\
	\
	zlib/adler32.o zlib/compress.o zlib/crc32.o zlib/gzio.o 	\
	zlib/uncompr.o zlib/deflate.o zlib/trees.o zlib/zutil.o 	\
	zlib/inflate.o zlib/infback.o zlib/inftrees.o zlib/inffast.o	\
	\
	libpng/png.o libpng/pngset.o libpng/pngget.o libpng/pngrutil.o  \
	libpng/pngtrans.o libpng/pngwutil.o libpng/pngread.o		\
	libpng/pngrio.o libpng/pngwio.o libpng/pngwrite.o		\
	libpng/pngrtran.o libpng/pngwtran.o libpng/pngmem.o		\
	libpng/pngerror.o libpng/pngpread.o				\
	\
	jpeg/tinyjpeg.o jpeg/jidctflt.o	jpeg/decode1.o jpeg/decode3.o   \
	jpeg/rgb24.o jpeg/bgr24.o jpeg/yuv420p.o jpeg/grey.o		\
	\
	sys/x86_init_fpu.o math/pow.o math/strtod.o

BINDIR   = /usr/bin
LIBDIR   = /usr/lib
AUXDIR   = $(LIBDIR)/syslinux
INCDIR   = /usr/include
COM32DIR = $(AUXDIR)/com32

all: libcom32.a

libcom32.a : $(LIBOBJS)
	rm -f $@
	$(AR) cq $@ $^
	$(RANLIB) $@

tidy:
	rm -f *.o .*.d */*.o */.*.d lib/sys/vesa/alphatbl.c

clean: tidy
	rm -f *.a

spotless: clean
	rm -f *~ \#* */*~ */\#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libcom32.a com32.ld $(INSTALLROOT)$(COM32DIR)
	-rm -rf $(INSTALLROOT)$(COM32DIR)/include
	cp -r ../include $(INSTALLROOT)$(COM32DIR)

# These files are performance critical, and doesn't compile well with -Os
sys/vesa/drawtxt.o: sys/vesa/drawtxt.c
	$(CC) $(CFLAGS) -O3 -c -o $@ $<

jpeg/jidctflt.o: jpeg/jidctflt.c
	$(CC) $(CFLAGS) -O3 -c -o $@ $<

-include .*.d */.*.d
