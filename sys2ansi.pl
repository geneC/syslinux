#!/usr/bin/perl
# $Id$
#
# Perl script to convert a Syslinux-format screen to PC-ANSI
#
@ansicol = (0,4,2,6,1,5,3,7);

while ( read(STDIN, $ch, 1) > 0 ) {
    if ( $ch eq "\x1A" ) {	# <SUB> <Ctrl-Z> EOF
	last;
    } elsif ( $ch eq "\x0C" ) {	# <FF>  <Ctrl-L> Clear screen
	print "\x1b[2J";
    } elsif ( $ch eq "\x0F" ) {	# <SI>  <Ctrl-O> Attribute change
	if ( read(STDIN, $attr, 2) == 2 ) {
	    $attr = hex $attr;
	    print "\x1b[0;";
	    if ( $attr & 0x80 ) {
		print "7;";
		$attr &= ~0x80;
	    }
	    if ( $attr & 0x08 ) {
		print "1;";
		$attr &= ~0x08;
	    }
	    printf "%d;%dm",
	    $ansicol[$attr >> 4] + 40, $ansicol[$attr & 7] + 30;
	}
    } else {
	print $ch;
    }
}

