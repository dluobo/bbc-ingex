#!/usr/bin/perl -w
#
# $Id: jpeg_info.pl,v 1.2 2009/02/26 19:10:22 john_f Exp $
#
use IO::Seekable;
use Getopt::Long;
use strict;

my %summary;
my $opt_summary = 0;
my $opt_quiet = 0;
my $opt_dqt_single_line = 0;
my $opt_display_bitstream_dqt = 0;
GetOptions('s', \$opt_summary, 'q', \$opt_quiet,
			'oneline', \$opt_dqt_single_line,
			'bitstream_dqt', \$opt_display_bitstream_dqt,
			);

die "Need filename arg" if ! scalar(@ARGV);

for my $file (@ARGV)
{
	dumpJPEG($file);
}

sub dumpJPEG
{
	my ($file) = @_;
	open(my $IN, $file) || die "$file: $!";

	my $last_avid_size = 0;
	while (1)
	{
		my ($mark, $len) = read_next_marker($IN);
		if (! defined $len) {
			printf "[End of file at pos=%d]\n", tell($IN);
			last;
		}

		printf("pos=%08X %10d mark=0x%02X length=%6d 0x%X\n",
					tell($IN), tell($IN), $mark, $len, $len);

		# End of image marker
		if ($mark == 0xD9) {
			print "    [End of image]\n\n";
			next;
		}

		next if ($len == 0);						# standalone markers

		# subtract 2 from $len since we have already read 2 bytes
		if ($opt_quiet) {
			seek($IN, $len - 2, SEEK_CUR) || die "seek($len-2): $!";
			next;
		}

		# Dump contents of segments
		my $buf;
		printf "%s=", marker_code($mark);
		read($IN, $buf, $len - 2) || die "read($len-2): $!";
		map { printf "%02x", ord($_) } split(//, $buf);
		print "\n";

		# Dump APPx markers
		if (marker_code($mark) =~ /^APP/) {
			# Dump Avid specific APP0 fields
			if (substr($buf, 0, 4) eq "AVI1") {
				my ($len1, $len2) = unpack('NN', substr($buf, 6));
				printf "    AVI1: 0x%x 0x%x (%d %d)\n", $len1, $len2, $len1, $len2;
				$last_avid_size = $len1;
			}
			hexdump($buf);
		}

		# Dump COM marker
		if (marker_code($mark) eq 'COM') {
			# Dump Avid specific COM fields
			if (substr($buf, 0, 5) eq "AVID\x11") {
				my ($size, $resid) = unpack('NC', substr($buf, 7));
				my %res_name = (	0x4b => '10:1 MJPEG',
									0x4c => '2:1 MJPEG',
									0x4d => '3:1 MJPEG',
									0x4e => '15:1 MJPEG',
									0x52 => '20:1 MJPEG');
				printf "    AVID: 0x%x (%d)  ResId=0x%x (%d) %s\n",
							$size, $size, $resid, $resid,
					(exists $res_name{$resid} ? $res_name{$resid} : 'Unknown');
			}
			hexdump($buf);
		}

		# display DQT quantization table
		if (marker_code($mark) eq 'DQT') {
			my $table = $buf;
			while (length($table)) {
				my ($bpe, $dest_id) = dumpDQT($table);
				my $current_table = substr($table, 0, 1 + 64 * $bpe);
				$summary{$dest_id}{$current_table}{count}++;
				push @{ $summary{$dest_id}{$current_table}{size} }, $last_avid_size;

				$table = substr($table, 1 + 64 * $bpe);
			}
		}

		# display DHT Huffman table
		if (marker_code($mark) eq 'DHT') {
			my $table = $buf;
			while (length($table)) {
				my ($TcTh, $Li, $Vij) = unpack('CC16C256', $table);
				printf "    Tc=%d Th=%d", $TcTh >> 4, $TcTh & 0x0f;
				printf "  (table class=%s  destination id=%d)\n",
							($TcTh >> 4) ? "AC" : "DC", $TcTh & 0x0f;
				print "      Li[] = ";
				my $len = 0;
				my @L;
				for my $i (0 .. 15) {
					$L[$i] = ord(substr($table, 1 + $i, 1));
					printf "%02x ", $L[$i];
					$len += $L[$i];
				}
				print "\n";
				$table = substr($table, (1+16+$len));
			}
		}

		# display frame header parameters for SOFn segments
		if (marker_code($mark) =~ /^SOF/) {
			my ($Pre, $Y, $X, $Nf, $comp) = unpack('CnnCa*', $buf);
			printf "     Precision=%d\n", $Pre;
			printf "     Y=%d X=%d\n", $Y, $X;
			printf "     Number of image components=%d\n", $Nf;
			for my $i (1 .. $Nf) {
				my ($Ci, $HVi, $Tqi, $remain) = unpack('CCCa*', $comp);
				$comp = $remain;
				printf "     CompId=%d", $Ci;
				printf " (HxV) subsampling=(%dx%d)",
							$HVi >> 4, $HVi & 0x0f;
				printf " Quant table dest selector=%d\n", $Tqi;
			}
		}

		# display Scan header data
		if (marker_code($mark) eq 'SOS') {
			my ($Ns, $comp) = unpack('Ca*', $buf);
			printf "    Number of scan components=%d\n", $Ns;
			my $len_of_all_comp = $Ns * 2;
			my ($compj, $Ss, $Se, $AhAl) = unpack("a$len_of_all_comp" . 'CCC', $comp);
			for my $i (1 .. $Ns) {
				my ($Csj, $TdjTaj, $remain) = unpack('CCa*', $compj);
				$compj = $remain;
				printf "      CompId=%d", $Csj;
				printf " Td=%d Ta=%d (DC and AC entropy coding table dest ids)\n", $TdjTaj >> 4, $TdjTaj & 0x0f;
			}
			printf "    Ss=%d Se=%d AhAl=0x%02x\n", $Ss, $Se, $AhAl;
		}

		if ($mark == 0xDA) {		# Start of scan marker
									# we don't decode the entropy coded segments
			printf "[Entropy coded segments] offset=0x%x  %d\n", tell($IN), tell($IN);
		}
	}
}

if ($opt_summary) {
	# dump summary of DQT segments for all JPEG images
	for my $dest (sort keys %summary) {
		for my $table (sort keys %{ $summary{$dest} }) {
			printf "count=%d  sizes=", $summary{$dest}{$table}{count};
			for my $s (@{ $summary{$dest}{$table}{size} }) {
				printf "%d ", $s;
			}
			printf "\n";
			dumpDQT($table);
			printf "\n";
		}
	}
}

=pod
 1  2  6  7 15 16 28 29    zig-zag ordering
 3  5  8 14 17 27 30 43
 4  9 13 18 26 31 42 44
10 12 19 25 32 41 45 54
11 20 24 33 40 46 53 55
21 23 34 39 47 52 56 61
22 35 38 48 51 57 60 62
36 37 49 50 58 59 63 64

 1  2  3  4  5  6  7  8    If this table is input to jpeg_add_quant_table()
 9 10 11 12 13 14 15 16    libjpeg will order it following zig-zag order
17 18 19 20 21 22 23 24
25 26 27 28 29 30 31 32
33 34 35 36 37 38 39 40
41 42 43 44 45 46 47 48
49 50 51 52 53 54 55 56
57 58 59 60 61 62 63 64

 1  2  9 17 10  3  4 11    resulting in this displayed output by old jpeg_info.pl
18 25 33 26 19 12  5  6
13 20 27 34 41 49 42 35
28 21 14  7  8 15 22 29
36 43 50 57 58 51 44 37
30 23 16 24 31 38 45 52
59 60 53 46 39 32 40 47
54 61 62 55 48 56 63 64
=cut

sub dqtidx
{
	my ($i) = @_;
	return $1 if $opt_display_bitstream_dqt;

	# display DQT in zig-zag order for input to libjpeg
	my @tab = qw(
 1  2  6  7 15 16 28 29
 3  5  8 14 17 27 30 43
 4  9 13 18 26 31 42 44
10 12 19 25 32 41 45 54
11 20 24 33 40 46 53 55
21 23 34 39 47 52 56 61
22 35 38 48 51 57 60 62
36 37 49 50 58 59 63 64
);
	return $tab[$i] - 1;
}

sub dumpDQT
{
	my ($table) = @_;

	my ($PqTq) = unpack('C', $table);
	my $dest_id = $PqTq & 0x0f;
	printf("    Pq=%d Tq=%d", $PqTq >> 4, $dest_id) unless $opt_dqt_single_line;
	my $bpe = ($PqTq >> 4) ? 2 : 1;
	printf("  (element precision=%d  destination id=%d)\n", $bpe * 8, $dest_id) unless $opt_dqt_single_line;
	for my $i (0 .. 63) {
		print "      " if $i % 8 == 0;
		if ($bpe == 1) {    # 8 bit precision
			printf "%3d ", ord(substr($table, 1 + dqtidx($i), 1));
		}
		else {              # 16 bit precision
			printf "%5d ", unpack("n", substr($table, 1 + $i*2, 2));
		}
		if (! $opt_dqt_single_line) {
			print "\n" if $i % 8 == 7;
		}
	}
	print "\n" if $opt_dqt_single_line;
	return ($bpe, $dest_id);
}

sub read_next_marker
{
	my ($IN) = @_;

	my $marker;
	my $buf;

	# Find 0xFF followed by a non-0x00 marker byte
	my $prev_ff = 0;
	while (1) {
		my $pos = tell($IN);
		read($IN, $buf, 1) || return (undef, undef);

		if (ord($buf) == 0xff) {
			$prev_ff = 1;
			next;
		}

		if ($prev_ff && ord($buf) != 0x00) {
			$marker = ord($buf);
			last;
		}
		$prev_ff = 0;
	}

	# D8 and D9 are standalone markers without any parameter
	if ($marker == 0xD8 || $marker == 0xD9) {
		return ($marker, 0);
	}

	# All other markers have a parameter of length given by next 2 bytes
	read($IN, $buf, 2) || die "Could not read marker length: $!";
	my $length = unpack('n', $buf);

	return ($marker, $length);
}

sub marker_code
{
	my ($code) = @_;

	my %table = (
		0xC0 => 'SOF0',		# Baseline DCT
		0xC1 => 'SOF1',		# Extended sequential DCT
		0xC2 => 'SOF2',		# Progressive DCT
		0xC3 => 'SOF3',		# Lossless (sequential)

		0xC4 => 'DHT',		# Define Huffman table(s)

		0xC5 => 'SOF5',		# Differential sequential DCT
		0xC6 => 'SOF6',		# Differential progressive DCT
		0xC7 => 'SOF7',		# Differential lossless (sequential)

		0xC8 => 'JPG',		# Reserved for JPEG extensions
		0xC9 => 'SOF9',		# Extended sequential DCT
		0xCA => 'SOF10',	# Progressive DCT
		0xCB => 'SOF11',	# Lossless (sequential)

		0xCC => 'DAC',		# Define arithmetic coding conditioning(s)

		0xCD => 'SOF13',	# Differential sequential DCT
		0xCE => 'SOF14',	# Differential progressive DCT
		0xCF => 'SOF15',	# Differential lossless (sequential)

							# D0 through D7 RSTm
		0xD0 => 'RST0',		# (standalone) Restart with modula 8 count 'm'
		0xD1 => 'RST1',
		0xD2 => 'RST2',
		0xD3 => 'RST3',
		0xD4 => 'RST4',
		0xD5 => 'RST5',
		0xD6 => 'RST6',
		0xD7 => 'RST7',

		0xD8 => 'SOI',		# (standalone) Start of Image
		0xD9 => 'EOI',		# (standalone) End of Image
		0xDA => 'SOS',		# Start of scan
		0xDB => 'DQT',		# Define quantization table(s)
		0xDC => 'DNL',		# Define number of lines
		0xDD => 'DRI',		# Define restart interval
		0xDE => 'DHP',		# Define hierarchical progression
		0xDF => 'EXP',		# Expand reference component(s)
							# 0xE0 through 0xEF
		0xE0 => 'APP0',		# Reserved for Application segments
		0xE1 => 'APP1',
		0xE2 => 'APP2',
		0xE3 => 'APP3',
		0xE4 => 'APP4',
		0xE5 => 'APP5',
		0xE6 => 'APP6',
		0xE7 => 'APP7',
		0xE8 => 'APP8',
		0xE9 => 'APP9',
		0xEA => 'APP10',
		0xEB => 'APP11',
		0xEC => 'APP12',
		0xED => 'APP13',
		0xEE => 'APP14',
		0xEF => 'APP15',
		0xF0 => 'JPG0',		# 0xF0 - 0xFD Reserved for JPEG extensions
		0xF1 => 'JPG1',
		0xF2 => 'JPG2',
		0xF3 => 'JPG3',
		# ...
		0xFB => 'JPG11',
		0xFC => 'JPG12',
		0xFD => 'JPG13',
		0xFE => 'COM',		# Comment

		0x01 => 'TEM',		# (standalone) temporary private use in arithmetic coding
	);
	# 0x02 through 0xBF		'RES' Reserved

	return $table{$code};
}

sub asciidump
{
	my ($buf) = @_;
	for (my $i = 0; $i < length $buf; $i++) {
		my $c = ord(substr($buf, $i, 1));
		printf "%s", ($c < 32 || $c > 126) ? '.' : chr($c);
	}
}

sub hexdump
{
	my ($buf) = @_;

	my $i;
	for ($i = 0; $i < length $buf; $i++) {
		print "    " if ($i % 16 == 0);
		printf "%02x ", ord(substr($buf, $i, 1));
		if ($i % 16 == 15) {
			print " ";
			asciidump(substr($buf, $i - 15, 16));
			print "\n";
		}
	}
	return if ($i % 16 == 0);

	my $leftover = $i % 16;
	print "   " x (16 - $leftover), " ";
	asciidump(substr($buf, $i - $leftover, $leftover));
	print "\n";
}
