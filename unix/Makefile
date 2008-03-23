TMPFILE = $(shell mktemp /tmp/gcc_ok.XXXXXX)

gcc_ok   = $(shell tmpf=$(TMPFILE); if $(CC) $(1) ../dummy.c -o $$tmpf 2>/dev/null; \
	           then echo '$(1)'; else echo '$(2)'; fi; rm -f $$tmpf)

comma   := ,
LDHASH  := $(call gcc_ok,-Wl$(comma)--hash-style=both,)

CC	 = gcc
OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libinstaller
CFLAGS	 = -W -Wall -D_FILE_OFFSET_BITS=64 $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = $(LDHASH) -s

SRCS     = syslinux.c ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:..:../libinstaller

all: installer

tidy:
	-rm -f *.o *.i *.s *.a .*.d

clean: tidy
	-rm -f syslinux syslinux-nomtools

spotless: clean
	-rm -f *~

installer: syslinux syslinux-nomtools

syslinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

syslinux-nomtools: syslinux
	ln -f $< $@

%.o: %.c
	$(CC) -Wp,-MT,$@,-MMD,.$@.d $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

-include .*.d
