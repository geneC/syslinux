#!/usr/bin/perl
#
# checkov.pl
#
# Check NASM map output for overflow
#
# This assumes that a section for which start != vstart, both
# ranges need to be checked for overflow (true for SYSLINUX)
#

($in, $target) = @ARGV;

sub overlap($$$$) {
    my($s1,$e1,$s2,$e2) = @_;
    
    return 1 if ( $s2 < $e1 && $e2 > $s1 );
    return 1 if ( $s1 < $e2 && $e1 > $s2 );

    return 0;
}

open(IN, '<', $in) or die "$0: Cannot open input file: $in\n";

$section = undef;
while ( $line = <IN> ) {
    if ( $line =~ /^-/ ) {
	if ( $line =~ /^\-\-\-\- Section (\S+) / ) {
	    $section = $1;
	} else {
	    $section = undef;
	}
    } elsif ( defined($section) ) {
	if ( $line =~ /^length\:\s*(\S+)/ ) {
	    $length{$section} = hex $1;
	} elsif ( $line =~ /^start\:\s*(\S+)/ ) {
	    $start{$section} = hex $1;
	} elsif ( $line =~ /^vstart\:\s*(\S+)/ ) {
	    $vstart{$section} = hex $1;
	}
    }
}
close(IN);

$err = 0;

foreach $s ( keys(%start) ) {
    $sstart  = $start{$s};
    $svstart = $vstart{$s};
    $send    = $sstart + $length{$s};
    $svend   = $svstart + $length{$s};

    if ( $send > 0x10000 || $svend > 0x10000 ) {
	print STDERR "$target: 16-bit overflow on section $s\n";
	$err++;
    }

    foreach $o ( keys(%start) ) {
	next if ( $s ge $o );
	
	$ostart  = $start{$o};
	$ovstart = $vstart{$o};
	$oend    = $ostart + $length{$o};
	$ovend   = $ovstart + $length{$o};
	
	if ( overlap($sstart, $send, $ostart, $oend) ||
	     overlap($svstart, $svend, $ostart, $oend) ||
	     overlap($sstart, $send, $ovstart, $ovend) ||
	     overlap($svstart, $svend, $ovstart, $ovend) ) {
	    print STDERR "$target: section $s overlaps section $o\n";
	    $err++;
	}
    }
}

if ( $err ) {
    unlink($target);
    exit(1);
} else {
    exit(0);
}
