CC	 = gcc
OPTFLAGS = -g -Os
INCLUDES = -I./ -I../ -I../libfat/
CFLAGS	 = -W -Wall $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = -s
NASM     = nasm

SRCS     = 
OBJS	 = 

.SUFFIXES: .c .o .i .s .S .asm .bin .lst .com

VPATH = .:..:../libfat

all: syslinux.com

tidy:
	-rm -f *.o *.i *.s *.a .*.d *.lst

clean: tidy
	-rm -f syslinux.com

spotless: clean
	-rm -f *~

installer:

syslinux.com: syslinux.asm ../ldlinux.bss ../ldlinux.sys

.asm.com:
	$(NASM) $(INCLUDES) -f bin -o $@ -l $*.lst $<
