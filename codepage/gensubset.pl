#!/usr/bin/perl
#
# Generate a subset of the UnicodeData.txt file, available from
# ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt
#
# Usage:
#   gensubset.pl [subset files] < UnicodeData.txt > MiniUCD.txt
#

%need_these = ();

foreach $file (@ARGV) {
    open(F, '<', $file) or die;
    while (defined($line = <F>)) {
	$line =~ s/\s*(\#.*|)$//; # Remove comments and final blanks
	@f = split(/\s+/, $line);
	next if (scalar @f != 2);
	$need_these{hex $f[1]}++;
    }
    close(F);
}

while (defined($line = <STDIN>)) {
    ($v, $l) = split(/;/, $line, 2);
    if ($v =~ /^([0-9a-f]+)\-([0-9a-f]+)$/i) {
	$r1 = hex $1;
	$r2 = hex $2;
    } elsif ($v =~ /^([0-9a-f]+)$/i) {
	$r1 = $r2 = hex $1;
    } else {
	next;
    }
    for ($r = $r1; $r <= $r2; $r++) {
	printf "%04X;%s", $r, $l if ($need_these{$r});
    }
}

	
