#!/usr/bin/perl
#
# Read the "version" file and produce some macro declarations
#

use Fcntl;

($vfile, $vout, $def) = @ARGV;
sysopen(VERSION, $vfile, O_RDONLY) or die "$0: Cannot open $vfile\n";
$version = <VERSION>;
chomp $version;
close(VERSION);

unless ( $version =~ /^([0-9]+)\.([0-9]+)$/ ) {
    die "$0: Cannot parse version format\n";
}
$vma = $1+0; $vmi = $2+0;

sysopen(VI, $vout, O_WRONLY|O_CREAT|O_TRUNC)
    or die "$0: Cannot create $vout: $!\n";
print VI "$def VERSION \"$version\"\n";
print VI "$def VER_MAJOR $vma\n";
print VI "$def VER_MINOR $vmi\n";
close(VI);


