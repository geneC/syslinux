TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)
CC	= gcc

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) ../dummy.c -o $$tmpf 2>/dev/null; \
	           then echo '$(1)'; else echo '$(2)'; fi; rm -f $$tmpf)

comma   := ,
LDHASH  := $(call gcc_ok,-Wl$(comma)--hash-style=both,)

OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libinstaller
CFLAGS	 = -W -Wall -Wno-sign-compare -D_FILE_OFFSET_BITS=64 $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = $(LDHASH) # -s

SRCS     = main.c ../libinstaller/setadv.c \
	   ../core/extlinux_bss_bin.c ../core/extlinux_sys_bin.c
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:../libinstaller:../core

all: installer

tidy dist:
	-rm -f *.o *.i *.s *.a .*.d

clean: tidy
	-rm -f extlinux

spotless: clean
	-rm -f *~

installer: extlinux

extlinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -Wp,-MT,$@,-MMD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

-include .*.d
