#!/usr/bin/perl
## -----------------------------------------------------------------------
##   
##   Copyright 2001 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Bostom MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------
## $Id$

#
# Postprocess the memdisk binary.
#

($file) = @ARGV;

open(FILE, "+< $file\0") or die "$0: Cannot open file: $file\n";

@info = stat(FILE);
$size = $info[7];

$sectors = ($size + 511) >> 9;
$xsize = $sectors << 9;

seek(FILE, $size, SEEK_SET);

if ( $size != $xsize ) {
    # Pad to a sector boundary
    print FILE "\0" x ($xsize-$size);
}

seek(FILE, 0x1f1, SEEK_SET);	# setup_sects
# All sectors are setup except the first
print FILE pack("C", $sectors-1);

close(FILE);

