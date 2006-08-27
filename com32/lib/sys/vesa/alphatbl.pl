#!/usr/bin/perl
#
# Compute the alpha-blending table for 256 possible (R, G, B) value
# compared with the 4-bit intensity value from the character attribute
#

#
# Configurable parameters...
#
@text_intensity = (0.0, 0.3333, 0.6667, 1.0);
$text_alpha = 0.5;
$input_gamma  = 1.7;
$text_gamma   = 1.7;
$output_gamma = 1.7;

sub ungamma($$) {
    my($v, $gamma) = @_;

    return $v**$gamma;
}

sub gamma($$) {
    my($v, $gamma) = @_;
    return $v**(1/$gamma);
}

print "unsigned char __vesacon_alpha_tbl[256][4] = {\n";

for ($i = 0; $i <= 255; $i++) {
    $ival = ungamma($i/255, $input_gamma);

    $intro = "\t{";

    for ($j = 0; $j < 4; $j++) {
	$v = ($ival*(1-$text_alpha)) +
	    ungamma($text_intensity[$j], $text_gamma)*$text_alpha;

	$d = int(gamma($v,$output_gamma)*255+0.5);

	$d = 0   if ($d < 0);
	$d = 255 if ($d > 255);

	printf "%s%3d", $intro, $d;
	$intro = ', ';
    }
    print "},\n";
}
print "};\n";
