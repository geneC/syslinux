#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 1998 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

#
# This file is part of the SYSLINUX compilation sequence.
#

undef $addr, $begin, $end;
$isbegin = 0;  $isend = 0;

while ( $line = <STDIN> ) {
    if ( $line =~ /^\s*([0-9]+) ([0-9A-F]{8}) / ) {
	$addr = hex $2;
	if ( $isbegin ) {
	    $begin = $addr;
	    $isbegin = 0;
	}
	if ( $isend ) {
	    $end = $addr;
	    $isend = 0;
	}
    }
    if ( $line =~ /^[0-9A-F\s]+__BEGIN_STUPID_PATCH_AREA\:/ ) {
	$isbegin = 1;
    } elsif ( $line =~ /^[0-9A-F\s]+__END_STUPID_PATCH_AREA\:/ ) {
	$isend = 1;
    }
}

if ( !defined($begin) || !defined($end) || $end > 512 || ($end-$begin) < 3 ) {
    print STDERR "$0: error locating STUPID_PATCH_AREA\n";
    exit 1;
}

open(CFILE, "> stupid.c") || die "$0: cannot create stupid.c: $!\n";

$addr = $begin;
printf CFILE "extern unsigned char bootsect[];\n";
printf CFILE "void make_stupid(void)\n";
printf CFILE "{\n";
printf CFILE "\tbootsect[0x%x] = 0xbd;\n", $addr++;
printf CFILE "\tbootsect[0x%x] = 0x01;\n", $addr++;
printf CFILE "\tbootsect[0x%x] = 0x00;\n", $addr++;
while ( $addr < $end ) {
    printf CFILE "\tbootsect[0x%x] = 0x90;\n", $addr++;
}
print CFILE "}\n";

close(CFILE);

open(ASMFILE, "> stupid.inc") || die "$0: cannot open stupid.inc: $!\n";

printf ASMFILE "\tsection .text\n";
printf ASMFILE "make_stupid:\n";
printf ASMFILE "\tmov si, .stupid_patch\n";
printf ASMFILE "\tmov di, BootSector+0%Xh\n", $begin;
printf ASMFILE "\tmov cx, %d\n", $end-$begin;
printf ASMFILE "\trep movsb\n";
printf ASMFILE "\tret\n";
printf ASMFILE "\tsection .data\n";
printf ASMFILE ".stupid_patch:\n";
printf ASMFILE "\tmov bp,1\n";
for ( $addr = $begin + 3 ; $addr < $end ; $addr++ ) {
    printf ASMFILE "\tnop\n";
}

close(ASMFILE);
