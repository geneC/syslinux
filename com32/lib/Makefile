# Include configuration rules
include MCONFIG

LIBOBJS = abort.o atexit.o atoi.o atol.o atoll.o calloc.o creat.o	\
	ctypes.o errno.o fgetc.o fgets.o fopen.o fprintf.o fputc.o	\
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
	vsscanf.o libgcc/__ashldi3.o libgcc/__udivdi3.o			\
	libgcc/__negdi2.o libgcc/__ashrdi3.o libgcc/__lshrdi3.o		\
	libgcc/__muldi3.o libgcc/__udivmoddi4.o libgcc/__umoddi3.o	\
	libgcc/__divdi3.o libgcc/__moddi3.o sys/entry.o sys/exit.o	\
	sys/fileinfo.o sys/opendev.o sys/read.o sys/write.o		\
	sys/close.o sys/open.o sys/fileread.o sys/fileclose.o		\
	sys/isatty.o sys/openconsole.o sys/line_input.o			\
	sys/stdcon_read.o sys/stdcon_write.o sys/rawcon_read.o		\
	sys/rawcon_write.o sys/err_read.o sys/err_write.o		\
	sys/null_read.o sys/null_write.o sys/serial_write.o		\
	sys/ansicon_write.o sys/ansiserial_write.o

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
	rm -f *.o .*.d */*.o */.*.d

clean: tidy
	rm -f *.a

spotless: clean
	rm -f *~ \#* */*~ */\#*

install: all
	mkdir -m 755 -p $(INSTALLROOT)$(COM32DIR)
	install -m 644 libcom32.a com32.ld $(INSTALLROOT)$(COM32DIR)
	-rm -rf $(INSTALLROOT)$(COM32DIR)/include
	cp -r ../include $(INSTALLDOOR)$(COM32DIR)

-include .*.d
