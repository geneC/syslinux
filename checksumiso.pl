#!/usr/bin/perl
#
# Construct a checksum for isolinux*.bin, compatible
# with an mkisofs boot-info-table
#

use bytes;
use integer;

($file) = @ARGV;

open(FILE, '+<', $file) or die "$0: Cannot open $file: $!\n";
binmode FILE;

if ( !seek(FILE,64,0) ) {
    die "$0: Cannot seek past header\n";
}

$csum  = 0;
$bytes = 64;
while ( ($n = read(FILE, $dw, 4)) > 0 ) {
    $dw .= "\0\0\0\0";		# Pad to at least 32 bits
    ($v) = unpack("V", $dw);
    $csum  = ($csum + $v) & 0xffffffff;
    $bytes += $n;
}

if ( !seek(FILE,16,0) ) {
    die "$0: Cannot seek to header\n";
}

print FILE pack("VV", $bytes, $csum);

close(FILE);

exit 0;
