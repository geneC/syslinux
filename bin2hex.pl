#!/usr/bin/perl
$len = 0;
while ( read(STDIN,$ch,1) ) {
    $cc = ord($ch);
    $len += printf ("%x", $cc);
    if ( $len > 72 ) {
	print "\n";
	$len = 0;
    } else {
	print " ";
	$len++;
    }
}
print "\n" if ( $len );
exit 0;

