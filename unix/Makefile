CC	 = gcc
OPTFLAGS = -g -Os
INCLUDES = -I. -I.. -I../libfat
CFLAGS	 = -W -Wall -D_FILE_OFFSET_BITS=64 $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -s

SRCS     = syslinux.c ../syslxmod.c ../bootsect_bin.c ../ldlinux_bin.c $(wildcard ../libfat/*.c)
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:..:../libfat

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



