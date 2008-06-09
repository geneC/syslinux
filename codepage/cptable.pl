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

%ucase   = ();
%lcase   = ();
%tcase   = ();

open(UCD, '<', $ucd) or die;
while (defined($line = <UCD>)) {
    chomp $line;
    @f = split(/;/, $line);
    $n = hex $f[0];
    $ucase{$n} = hex $f[12] if ($f[12] ne '');
    $lcase{$n} = hex $f[13] if ($f[13] ne '');
    $tcase{$n} = hex $f[14] if ($f[14] ne '');
}
close(UCD);

@xtab = (undef) x 256;
%tabx = ();

open(CPIN, '<', $cpin) or die;
while (defined($line = <CPIN>)) {
    $line =~ s/\s*(\#.*|)$//;
    @f = split(/\s+/, $line);
    next if (scalar @f != 2);
    next if (hex $f[0] > 255);
    $xtab[hex $f[0]] = hex $f[1]; # Codepage -> Unicode
    $tabx{hex $f[1]} = hex $f[0]; # Unicode -> Codepage
}
close(CPIN);

open(CPOUT, '>', $cpout) or die;
#
# Magic number, in anticipation of being able to load these
# files dynamically...
#
print CPOUT pack("VV", 0x8fad232b, 0x9c295319);

# Header fields available for future use...
print CPOUT pack("VVVVVV", 0, 0, 0, 0, 0, 0);

#
# Self (shortname) uppercase table
#
for ($i = 0; $i < 256; $i++) {
    $u = $tabx{$ucase{$xtab[$i]}};
    $u = $i unless (defined($u));
    print CPOUT pack("C", $u);
}

#
# Unicode (longname) matching table
#
for ($i = 0; $i < 256; $i++) {
    if (!defined($xtab[$i])) {
	$p0 = $p1 = 0xffff;
    } else {
	$p0 = $xtab[$i];
	if (defined($ucase{$p0})) {
	    $p1 = $ucase{$p0};
	} elsif (defined($lcase{$p0})) {
	    $p1 = $lcase{$p0};
	} elsif (defined($tcase{$p0})) {
	    $p1 = $tcase{$p0};
	} else {
	    $p1 = $p0;
	}
    }
    # Only the BMP is supported...
    $p0 = 0xffff if ($p0 > 0xffff);
    $p1 = 0xffff if ($p1 > 0xffff);
    print CPOUT pack("vv", $p0, $p1);
}
close (CPOUT);

    
