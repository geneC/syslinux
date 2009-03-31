## -----------------------------------------------------------------------
##
##   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## checksize.pl
##
## Check the size of a binary file and pad it with zeroes to that size
##

use bytes;

($file, $maxsize, $padsize) = @ARGV;

$padsize = $maxsize unless(defined($padsize));

open(FILE, '+<', $file) or die;
@st = stat(FILE);
if (!defined($size = $st[7])) {
    die "$0: $file: $!\n";
}
if ($size > $maxsize) {
    print STDERR "$file: too big ($size > $maxsize)\n";
    exit 1;
} elsif ($size < $padsize) {
    seek(FILE, $size, 0);
    print FILE "\0" x ($padsize-$size);
}

exit 0;
