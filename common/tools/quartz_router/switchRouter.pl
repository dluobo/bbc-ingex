#!/usr/bin/perl -w
# $Id: switchRouter.pl,v 1.1 2008/09/16 09:38:05 stuart_hc Exp $
use Getopt::Std;
use strict;

# Get options from the command line
# $opt_b -> duration of blank (seconds)
# $opt_o -> video on for (seconds)
# $opt_d -> router destination port
# $opt_s -> router source port
# $opt_p -> serial port
# $opt_v -> verbose

# Declare global variables written to by getopts()
our($opt_b, $opt_o, $opt_d, $opt_s, $opt_p, $opt_v);

$opt_v = 0;

if (!getopts('b:d:o:p:s:v')) {
	print "switchRouter.pl [options] [on|off]
	-b <time>  blank video for <time> seconds (default 3)
	-d <dest>  router output port (default 1)
	-o <time>  video on for <time> seconds (default 5)
	-p <port>  use serial port <port> (default \"/dev/ttyS0\")
	-s <src>   router input port (default 1)
	-v         verbose\n";
	exit;
}

# Note that the serial port needs to be set to 38400 baud for the Quartz router
# use sudo stty -F /dev/ttyS0 38400
my $port = $opt_p || "/dev/ttyS0";

my $onFor = $opt_o || 5; # default to 5 seconds on
my $offFor = $opt_b || 3; # default to 3 seconds off

my $srcPort  = $opt_s || 1;
my $destPort = $opt_d || 1;
my $noSignalPort = 13;		# this router port assumed to have no valid video signal

# Strings sent to router to switch source.
# format is .SV<destination>,<source>\r
my $videoOn = ".SV$destPort,$srcPort\r";
my $videoOff = ".SV$destPort,$noSignalPort\r";

# Open serial port to router
open(DEV, ">$port") or die "Can't open $port\n";
if ($opt_v) {print "port $port opened\n"};

# To flush output stream
my $oldh = select(DEV);
$| = 1;

# check for optional single commands "on" and "off"
if (scalar @ARGV) {
	if ($opt_v) {print STDOUT "Switching video $ARGV[0]\n";}
	my $switch_command = ($ARGV[0] eq 'on') ? $videoOn : $videoOff;
	print DEV $switch_command;
	select($oldh); # flush
	sleep(1);
	# The first command usually doesn't work so repeat it
	print DEV $switch_command;
	select($oldh); # flush
	exit;
}

# For some reason the first command doesn't work so add an extra
# command so first intended command does work.
print DEV $videoOn;
select($oldh); # flush

# Loop forever switching video signal on and off.
while (1) {
	if ($opt_v) {print STDOUT "Video on for ${onFor}s...\n"};
	print DEV $videoOn;
	select($oldh); # flush
	sleep($onFor);
	
	if ($opt_v) {print STDOUT "Blanking video for ${offFor}s...\n"};
	print DEV $videoOff;
	select($oldh); # flush
	sleep($offFor);
}
