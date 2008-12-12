#!/usr/bin/perl
#
# Construct a checksum for isolinux*.bin, compatible
# with an mkisofs boot-info-table
#

use bytes;
use integer;

($file) = @ARGV;

open(FILE, '+<', $file) or die "$0: cannot open $file: $!\n";
binmode FILE;

@fstat = stat(FILE) or die "$0: stat $file: $!\n";
if (!$fstat[7]) {
    die "$0: $file: cannot query length\n";
}

# Pad file to a multiple of 2048 bytes
$frac = $fstat[7] % 2048;
if ($frac) {
    seek(FILE,$fstat[7],0)
	or die "$0: $file: cannot seek to end\n";
    print FILE "\0" x (2048-$frac);
}

# Checksum the file post header
if ( !seek(FILE,64,0) ) {
    die "$0: $file: cannot seek past header\n";
}

$csum  = 0;
$bytes = 64;
while ( ($n = read(FILE, $dw, 4)) > 0 ) {
    $dw .= "\0\0\0\0";		# Pad to at least 32 bits
    ($v) = unpack("V", $dw);
    $csum  = ($csum + $v) & 0xffffffff;
    $bytes += $n;
}

# Update header
if ( !seek(FILE,16,0) ) {
    die "$0: $file: cannot seek to header\n";
}

print FILE pack("VV", $bytes, $csum);

close(FILE);

exit 0;
