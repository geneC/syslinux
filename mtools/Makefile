topdir = ..
MAKEDIR = $(topdir)/mk
include $(MAKEDIR)/syslinux.mk

OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libfat -I../libinstaller
CFLAGS	 = $(GCCWARN) -D_FILE_OFFSET_BITS=64 $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = 

SRCS     = syslinux.c \
	   ../libinstaller/fat.c \
	   ../libinstaller/syslxmod.c \
	   ../libinstaller/syslxopt.c \
	   ../libinstaller/setadv.c \
	   ../libinstaller/bootsect_bin.c \
	   ../libinstaller/ldlinux_bin.c \
	   $(wildcard ../libfat/*.c)
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:../libfat:../libinstaller

all: installer

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d *.tmp

clean: tidy
	-rm -f syslinux

spotless: clean
	-rm -f *~

installer: syslinux

syslinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

strip:
	$(STRIP) syslinux

%.o: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -S -o $@ $<

-include .*.d *.tmp
