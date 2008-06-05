#!/usr/bin/perl
#
# Produce a codepage matching table.  For each 8-bit character, list
# a primary and an alternate match (the latter used for case-insensitive
# matching.)
#
# Usage:
#	cptable.pl UnicodeData cpXXX.txt cpXXX.bin
#

($ucd, $cpin, $cpout) = @ARGV;

%altcase = ();

open(UCD, '<', $ucd) or die;
while (defined($line = <UCD>)) {
    chomp $line;
    @f = split(/;/, $line);
    if ($f[12] ne '') {
	$altcase{hex $f[0]} = hex $f[12]; # Upper case equivalent
    } elsif ($f[13] ne '') {
	$altcase{hex $f[0]} = hex $f[13]; # Lower case equivalent
    } elsif ($f[14] ne '') {
	$altcase{hex $f[0]} = hex $f[14]; # Title case, would be unusual
    } else {
	$altcase{hex $f[0]} = hex $f[0];
    }
}
close(UCD);

@xtab = (undef) x 256;

open(CPIN, '<', $cpin) or die;
while (defined($line = <CPIN>)) {
    $line =~ s/\s*(\#.*|)$//;
    @f = split(/\s+/, $line);
    next if (scalar @f != 2);
    next if (hex $f[0] > 255);
    $xtab[hex $f[0]] = hex $f[1];
}
close(CPIN);

open(CPOUT, '>', $cpout) or die;
for ($i = 0; $i < 256; $i++) {
    if (!defined($xtab[$i])) {
	$p0 = $p1 = 0xffff;
    } else {
	$p0 = $xtab[$i];
	$p1 = defined($altcase{$p0}) ? $altcase{$p0} : $p0;
    }
    # Only the BMP is supported...
    $p0 = 0xffff if ($p0 > 0xffff);
    $p1 = 0xffff if ($p1 > 0xffff);
    print CPOUT pack("vv", $p0, $p1);
}
close (CPOUT);

    
