## -----------------------------------------------------------------------
##
##   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
## Check the size of a binary file
##

($file, $maxsize) = @ARGV;

@st = stat($file);
if (!defined($size = $st[7])) {
    die "$0: $file: $!\n";
}
if ($size > $maxsize) {
    print STDERR "$file: too big ($size > $maxsize)\n";
    exit 1;
} else {
    exit 0;
}
