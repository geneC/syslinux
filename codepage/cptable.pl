#!/usr/bin/perl
#
# Produce a codepage matching table.  For each 8-bit character, list
# a primary and an alternate match (the latter used for case-insensitive
# matching.)
#
# Usage:
#	cptable.pl UnicodeData console-cp.txt filesystem-cp.txt output.cp
#
# Note: for the format of the UnicodeData file, see:
# http://www.unicode.org/Public/UNIDATA/UCD.html
#

($ucd, $cpco, $cpfs, $cpout) = @ARGV;

if (!defined($cpout)) {
    die "Usage: $0 UnicodeData console-cp.txt fs-cp.txt output.cp\n";
}

%ucase   = ();
%lcase   = ();
%tcase   = ();
%decomp  = ();

open(UCD, '<', $ucd)
    or die "$0: could not open unicode data: $ucd: $!\n";
while (defined($line = <UCD>)) {
    chomp $line;
    @f = split(/;/, $line);
    $n = hex $f[0];
    $ucase{$n} = ($f[12] ne '') ? hex $f[12] : $n;
    $lcase{$n} = ($f[13] ne '') ? hex $f[13] : $n;
    $tcase{$n} = ($f[14] ne '') ? hex $f[14] : $n;
    if ($f[5] =~ /^[0-9A-F\s]+$/) {
	# This character has a canonical decomposition.
	# The regular expression rejects angle brackets, so other
	# decompositions aren't permitted.
	$decomp{$n} = [];
	foreach my $dch (split(' ', $f[5])) {
	    push(@{$decomp{$n}}, hex $dch);
	}
    }
}
close(UCD);

#
# Filesystem and console codepages.  The filesystem codepage is used
# for FAT shortnames, whereas the console codepage is whatever is used
# on the screen and keyboard.
#
@xtab = (undef) x 256;
%tabx = ();
open(CPFS, '<', $cpfs)
    or die "$0: could not open fs codepage: $cpfs: $!\n";
while (defined($line = <CPFS>)) {
    $line =~ s/\s*(\#.*|)$//;
    @f = split(/\s+/, $line);
    next if (scalar @f != 2);
    next if (hex $f[0] > 255);
    $xtab[hex $f[0]] = hex $f[1]; # Codepage -> Unicode
    $tabx{hex $f[1]} = hex $f[0]; # Unicode -> Codepage
}
close(CPFS);

@ytab = (undef) x 256;
%taby = ();
open(CPCO, '<', $cpco)
    or die "$0: could not open console codepage: $cpco: $!\n";
while (defined($line = <CPCO>)) {
    $line =~ s/\s*(\#.*|)$//;
    @f = split(/\s+/, $line);
    next if (scalar @f != 2);
    next if (hex $f[0] > 255);
    $ytab[hex $f[0]] = hex $f[1]; # Codepage -> Unicode
    $taby{hex $f[1]} = hex $f[0]; # Unicode -> Codepage
}
close(CPCO);

open(CPOUT, '>', $cpout)
    or die "$0: could not open output file: $cpout: $!\n";
#
# Magic number, in anticipation of being able to load these
# files dynamically...
#
print CPOUT pack("VV", 0x8fad232b, 0x9c295319);

# Header fields available for future use...
print CPOUT pack("VVVVVV", 0, 0, 0, 0, 0, 0);

#
# Self (shortname) uppercase table.
# This depends both on the console codepage and the filesystem codepage;
# the logical transcoding operation is:
#
# $tabx{$ucase{$ytab[$i]}}
#
# ... where @ytab is console codepage -> Unicode and
# %tabx is Unicode -> filesystem codepage.
#
for ($i = 0; $i < 256; $i++) {
    $uuc = $ucase{$ytab[$i]};	# Unicode upper case
    if (defined($tabx{$uuc})) {
	# Straight-forward conversion
	$u = $tabx{$uuc};
    } elsif (defined($tabx{${$decomp{$uuc}}[0]})) {
	# Upper case equivalent stripped of accents
	$u = $tabx{${$decomp{$uuc}}[0]};
    } else {
	# No equivalent at all found.  Set this to zero, which should
	# prevent shortname matching altogether (still making longname
	# matching possible, of course.)
	$u = 0;
    }
    print CPOUT pack("C", $u);
}

#
# Unicode (longname) matching table.
# This only depends on the console codepage.
#
$pp0 = '';  $pp1 = '';
for ($i = 0; $i < 256; $i++) {
    if (!defined($ytab[$i])) {
	$p0 = $p1 = 0xffff;
    } else {
	$p0 = $ytab[$i];
	if ($ucase{$p0} != $p0) {
	    $p1 = $ucase{$p0};
	} elsif ($lcase{$p0} != $p0) {
	    $p1 = $lcase{$p0};
	} elsif ($tcase{$p0} != $p0) {
	    $p1 = $tcase{$p0};
	} else {
	    $p1 = $p0;
	}
    }
    # Only the BMP is supported...
    $p0 = 0xffff if ($p0 > 0xffff);
    $p1 = 0xffff if ($p1 > 0xffff);
    $pp0 .= pack("v", $p0);
    $pp1 .= pack("v", $p1);
}
print CPOUT $pp0, $pp1;
close (CPOUT);

    
