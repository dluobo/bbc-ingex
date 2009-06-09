#! /usr/bin/perl -w

#/***************************************************************************
# * $Id: xferserver.pl,v 1.10 2009/06/09 13:44:10 john_f Exp $             *
# *                                                                         *
# *   Copyright (C) 2008-2009 British Broadcasting Corporation              *
# *   - all rights reserved.                                                *
# *   Authors: Philip de Nier, Matthew Marks                                *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
# ***************************************************************************/

# This script controls copying of material from the local host to a remote file server mounted as a network drive or via ftp, under the instruction of clients, applying switchable traffic control to prevent saturation of the local disks.

# In the following description, the capitalised words refer to constants defined below.

# The script listens on network port CLIENT_PORT for clients to open connections, send parameters and then close connections.  These clients will typically be Ingex recorders or Ingex players.
# Parameters are as follows:
# <client name> [<identifier> [<priority> <source path> <destination path>]] ... (each terminated with a newline)

# The client name and identifier combination unambiguously identify an event requiring traffic control, to cater for, e.g., recorders which can make more than one recording simultaneously.
# If no other parameters are supplied, this indicates the start of the event.  The name/identifier pair are added to an event list, and traffic control is switched on at rate SLOW_LIMIT for copying to mounted drives, and FTP_SPEED for copying via FTP when option -f is used.
# Additional parameters indicate the end of the event, or a recorder being started.  This will result in the event's identifier being removed from the event list (if present), and if the list is then empty, traffic control being switched off (reverting to copying at FAST_LIMIT for mounted drives, which is unlimited if set to zero, and unlimited for FTP).
# Identifier values are arbitrary (most likely an incrementing number) except for the special case '0' (zero), which is interpreted as covering all events belonging to the associated client.  This value should be sent when a client is started, so that the event list is cleared of any previous events belonging to this client.  This ensures that traffic control does not remain on permanently due to a client crashing and hence not indicating the end of an event.  It should not be used as an event identifier if the client can have more than one event in action simultaneously.
# A further special case is to provide the client name on its own.  This is also interpreted as clearing the list of events belonging to this client (as if '0' followed by more arguments had been supplied), and is useful for clients such as players which do not provide copying path information.  (Players typically only play one file set at once, which means that they can always supply the same identifier value to switch traffic control on.)

# Arguments after the client name and identifier should be supplied in groups of three: priority (a positive number), source path and destination path.  The script will attempt to copy all the files in all the path pairs at the highest priority (the smallest priority argument value) before moving on to the next, and so on.  (Offline files would normally be given higher priority [smaller argument value] than online files, because they are likely to be needed sooner and are smaller so will be copied more quickly.)  Paths are saved in configuration file PATHS_FILE to allow copying to commence without client intervention when the script is (re)started.  When a client supplies paths, the copying cycle starts or restarts from the highest priority, letting any pending file copy finish first.  This minimises the delay in copying high-priority files without wasting any bandwidth by re-copying.

# In each source directory, every file whose name begins with 8 digits* and ends in ".mxf" or ".mov" (case-insensitive) is examined.  If it does not exist in a subdirectory of the corresponding destination path, named after the first 8 digits of the filename*, it is copied using the COPY executable or curl (FTP mode), into <destination path>/<first 8 digits of filename>*/<DEST_INCOMING>, creating directories if necessary.  After a successful copy, the file is moved up one directory level.  This prevents anything looking at the destination directories from seeing partial files.  At the end of each copying cycle, the (empty) DEST_INCOMING subdirectories are removed.  (* If option -d is provided, 8-digit subdirectories are not used and files do not have to start with 8 digits.)

# If the file already exists in its final resting place, but is of a different length to the source file, a warning is issued and the destination file is renamed to allow copying without overwriting it.  (Copying is abandoned if there is already a file with the new name.)  If a file fails to copy, any partial file is deleted and no more files are copied from that source directory (because the order of copying must be maintained - see below).  Once a copy cycle has been successfully completed, the script sleeps until it receives further client connections.  If any files could not be copied, it re-awakes after RECHECK_INTERVAL to try again.
# Because the script creates all required destination directories, it is recommended that mount points are made non-writeable so that non-mounted drives are detected rather than destination directories being created at, and data being copied to, the mount point, which may fill up a system drive.

# For each priority, the files to copy are sorted by decreasing age as indicated by ctime, making sure that all files with the latest detected ctime have been accounted for.  Before copying a file, the amount of free space is checked to make sure it is likely to fit (unless in FTP mode), and further copying from this source directory is abandoned (until retry) if it doesn't: this avoids wasting time repeatedly copying large files that won't succeed.  When copying of the last file with a particular ctime in a particular source directory is completed, this ctime is written to the configuration file, and any files older than this are subsequently ignored.  This allows files to be removed from the destination without them being copied again if they remain on the source.  (ctime is used rather than mtime, because it indicates the time the source file was made visible by moving it into the source directory, therefore making it impossible for older files, which would never be copied, to appear subsequently.)
# There must not be more than one destination path for any source path (or the most recent destination path will apply).  When a path pair is supplied, an existing pair with the same source at another priority will be removed.  There is no way to remove path pairs automatically - these must be removed from PATHS_FILE while the script is not running.

# PATHS_FILE contains fields separated by newlines.  Each group of fields consists of a priority value (indicated by having a leading space), followed by one or more triplets of source directory, destination directory and ctime value.  Any files in the corresponding source directory with a ctime of this value or less will not be copied.  (The -c option causes these values to be set to zero when the file is first read, but updated values will be written to the file even if nothing is copied.)  An exception is the priority 1 section, which contains the extra directory name immediately after the priority value (may be blank) and an extra ctime value for this extra directory, after each normal ctime value.

# If option -e is supplied, the path specified with it forms an additional destination for all files at priority '1'.  This is intended for use with a portable drive onto which to copy offline rushes (and operates in the same way whether using FTP mode or not).

# As source directories are scanned, source files are deleted if they have been successfully copied (indicated by existing at the destination with the same size, or having a ctime older than the value stored in PATHS_FILE) their mtimes are more than KEEP_PERIOD old, and the source directory is at least DELETE_THRESHOLD percent full.

# The script also runs a very simple HTTP server.  This is configured to return JSON-formatted data at http://<host>:<MON_PORT>/ (status data) or .../availability (a "ping").

# NB This script will only work properly if the source directories are on a filing system that supports ctime.

use strict;
use IO::Socket;
use IO::Select;
use IO::File;
use Getopt::Std;
use IPC::ShareLite qw( :lock); #don't use Shareable because it doesn't work properly with nested hashes, not cleaning up shared memory or semaphores after itself.  This may need to be installed from CPAN
#the qw( :lock) imports LOCK_EX which shouldn't be needed as an argument to lock() but causes a warning if it isn't supplied
use Storable qw(freeze thaw); #to allow sharing of references
use Filesys::DfPortable; #this may need to be installed from CPAN
#use Data::Dumper;

use constant CLIENT_PORT => 2000;
use constant MON_PORT => 7010;
use constant DEST_INCOMING => 'incoming'; #incoming file subdirectory - note that this must match the value in makebrowsed.pl to ensure that directories for incoming files are ignored
use constant KEEP_PERIOD => 24 * 60 * 60 * 7; #a week (in seconds)
use constant DELETE_THRESHOLD => 90; #disk must be at least this full (%) for deletions to start
use constant PERMISSIONS => '0775'; #for directory creation
#use constant COPY => '/home/ingex/bin/cpfs'; #for copying at different speeds
use constant COPY => './cpfs'; #for copying at different speeds
use constant SLOW_LIMIT => 2000; #bandwidth limit as supplied to COPY
use constant FAST_LIMIT => 0; #bandwidth limit as supplied to COPY (0 = unlimited)
use constant RECHECK_INTERVAL => 10; #time (seconds) between rechecks if something failed
use constant PATHS_FILE => 'paths';

use constant FTP_ROOT => '';
use constant FTP_SPEED => '20'; #Mbits/sec

use constant UP_TO_DATE => 0;
use constant PROCESSING => 1;
use constant STALE => 2;
use vars qw($childPid %opts $curl $interface @ftpDetails);

die "Usage: xferserver.pl [-c] [-d] [-e <path>] [-f <server>] [-f '<server> <username> <password>'] [-p] [-r] [-s] [-v]
 -c  clear stored ctime values (re-copy anything not found on server)
 -d  do not make and copy to 8-digit subdirectories
 -e <path>  extra destination for priority 1 files
 -f '<server>[ <username> <password>]'  use ftp rather than copying (apart from to extra destination)
 -p  preserve files (do not delete anything)
 -r  reset (remove stored information)
 -s  start in slow (traffic-limited) mode - NB will switch to fast as soon as ANY client indicates the end of an event
 -v  verbose: print out every source file name with reasons for not copying, and put curl into verbose mode for ftp transfers\n"
unless getopts('cde:f:prsv', \%opts) && !scalar @ARGV && (!defined $opts{e} || ($opts{e} ne '' && $opts{e} !~ /^-/)) && (!defined $opts{f} || ($opts{f} !~ /^-/ && (@ftpDetails = split/\s+/, $opts{f}) && (1 == scalar @ftpDetails || 3 == scalar @ftpDetails)));

my %transfers;
$transfers{limit} = ($opts{'s'} |= 0); #quotes stop highlighting getting confused in kate!
if ($opts{f}) {
	FtpTrafficControl(0); #in case it's already on, which would result in an abort when we try to switch it on
	if ($transfers{limit}) {
		my $err = FtpTrafficControl(1);
		Abort("Failed to start FTP traffic control: $err") if $err ne '';
		print "TRAFFIC CONTROL ON\n";
	}
	#generate curl common options
	$curl = "curl --proxy ''";
	$curl .= " --user $ftpDetails[1]:$ftpDetails[2]" if 3 == scalar @ftpDetails; #non-anonymous
	if (system "curl --help | grep 'keepalive-time'") { #not found
		print "WARNING: curl doesn't support '--keepalive-time' option so long transfers may fail, depending on the characteristics of the network\n";
	}
	else {
		$curl .= ' --keepalive-time 600';
	}
	$curl .= ' -v' if $opts{v};
}
$transfers{pid} = 0;
$transfers{date} = 0; #need this to keep date value in sync when calling Report() from different threads
$transfers{startOfLine} = 1; #to prevent it printing a newline before the first message

my $share = IPC::ShareLite->new( #use a my variable and pass it as a parameter or the signal is not
	-create => 'yes',
	-destroy => 'yes',
	-size => 10240 #default size of 65k is a bit excessive for this app!
) or die $!;

#start the copying thread
#trap signals to ensure copying process is killed and shared memory segment is released
my $terminate;
$SIG{INT} = \&Abort;
$SIG{TERM} = \&Abort;
if (!defined ($childPid = open(CHILD, "|-"))) {
	die "failed to fork: $!\n";
}
elsif ($childPid == 0) { # child - copying
	&childLoop($share); #won't do anything until kicked
}
CHILD->autoflush(1);

#load the configuration
my $priority = 1;
if ($opts{r}) {
	unlink PATHS_FILE;
}
elsif (-e PATHS_FILE) {
	if (-s PATHS_FILE) {
		if (open FILE, PATHS_FILE) {
			print "Reading from '", PATHS_FILE, "'...\n";
			my @config = <FILE>;
			close FILE;
			chomp @config;
			$share->lock(LOCK_EX);
			#be strict in reading the file because mistakes could lead to data loss (wrong ctimeCleared causing files which haven't been copied to be deleted)
			my $ok = 1;
			while ($ok && @config) {
				my $priority = shift @config;
				unless ($priority =~ s/^ (\d+)$/$1/ && $priority > 0) {
					print "WARNING: invalid priority value: '$priority'\n";
					last;
				}
				my $extraDestMismatch = 0;
				if (1 == $priority) {
					if (@config) {
						if (exists $transfers{extraDest}) {
							print "WARNING: more than one priority 1 entry\n";
							last;
						}
						elsif ($opts{e} && $opts{e} ne $config[0]) {
							print "WARNING: Extra destination '$config[0]' does not match extra destination '$opts{e}' supplied: ctimeCleared values will be reset\n" if '' ne $config[0];
							$extraDestMismatch = 1;
						}
						$transfers{extraDest} = $opts{e} |= '';
						shift @config;
					}
					else {
						print "WARNING: No extra destination supplied\n";
						last;
					}
				}
				while ($ok && @config && $config[0] !~ /^ /) {
					my $sourceDir;
					$ok = addPair(\%transfers, \@config, $priority, $extraDestMismatch, $opts{c} |= 0); #get ctimeCleared from @config (unless clearing stored values)
				}
			}
			print "Ignoring rest of file\n" if @config;
			if (!exists $transfers{transfers}) {
				print "WARNING: No valid transfer pairs found\n\n";
			}
		}
		else {
			print "WARNING: Couldn't read from '", PATHS_FILE, "': $!\n";
		}
	}
	else {
		print "Empty '", PATHS_FILE, "' file.\n\n";
	}
}
else {
	print "No stored paths file '", PATHS_FILE, "' found.\n\n";
}

$share->store(freeze \%transfers);
$share->unlock;
my $ports = new IO::Select;

#open the socket for instructions from recorders via xferclient.pl
my $recSock = new IO::Socket::INET(
 	LocalPort => CLIENT_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . CLIENT_PORT . ": $!\n");
$ports->add($recSock);
Report("Listening for copying clients on port " . CLIENT_PORT . ".\n", 0, $share);

#open the socket for status requests from web browser-type things
my $monSock = new IO::Socket::INET(
 	LocalPort => MON_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . MON_PORT . ": $!\n");
$ports->add($monSock);
Report("Listening for status requests at http://localhost:" . MON_PORT . "/.\n\n", 0, $share);

my %events;

#main loop
while (1) {
	my @requests = $ports->can_read;
	foreach (@requests) {
 		my $client = $_->accept();
		if ($_ == $recSock) {
			&serveClient($client, $share);
		}
		else {
			&serveStatus($client, $share);
		}
		$client->shutdown(2); # stopped using it - this seems to prevent partial status pages being served (when close() is used instead)
	}
}

# handle commands from a client, modifying the transfers hash and controlling traffic
sub serveClient {
 my ($client, $share) = @_;
 my ($buf, $data);
 while (sysread($client, $buf, 100)) { # don't use buffered read or can hang if several strings arrive at once
	$data .= $buf;
 }
 if (!defined $data) {
	Report("WARNING: Client connection made but no parameters passed: ignoring.\n", 0, $share);
	return;
 }
 my @commands = split /\n/, $data;
 $client = shift @commands;
 my $identifier = shift @commands;
 $share->lock(LOCK_EX);
 my $transfers = thaw($share->fetch);
 if (@commands || !defined $identifier) { # event end, from recorder or player
	if (!defined $identifier || $identifier eq '0') { #all events
		Report("Client '$client' is clearing all events.\n", 1, $transfers);
		delete $events{$client};
	}
	else {
		Report("Client '$client' has finished an event (identifier '$identifier').\n", 1, $transfers);
		delete $events{$client}{$identifier};
		if (!keys %{ $events{$client} }) {
			delete $events{$client};
		}
	}
	my $added = 0;
	if (@commands) {
		Report("Client '$client' says copy:\n", 1, $transfers);
		do {
			$added = 1 if 2 == addPair($transfers, \@commands, shift @commands, 0, 2); #printing during this will not be interspersed with printing from the thread as the share is locked
		} while (@commands);
	}
	if (!keys %events && $transfers->{limit}) {
		$transfers->{limit} = 0;
		kill 'USR2', $transfers->{pid} if $transfers->{pid};
		if ($opts{f}) {
			my $err = FtpTrafficControl(0);
			Report("WARNING: failed to remove FTP traffic control: $err", 1, $transfers) if $err ne '';
		}
		print "\rTRAFFIC CONTROL OFF\n";
	}
	if ($added) {
		SaveConfig($transfers); #so that if the script is re-started it can carry on copying immediately
	}
 }
 else { #event start
	#traffic control
	$events{$client}{$identifier} = 1;
	Report("Client '$client' has started an event (identifier '$identifier').\n", 1, $transfers);
	if (!$transfers->{limit}) { #not already controlling traffic
		if ($opts{f}) {
			my $err = FtpTrafficControl(1);
			Abort("Failed to start FTP traffic control: $err") if $err ne '';
		}
		$transfers->{limit} = 1;
		kill 'USR1', $transfers->{pid} if $transfers->{pid};
		print "\rTRAFFIC CONTROL ON\n";
	}
 }
 $share->store(freeze $transfers);
 $share->unlock;
}

# return HTTP status data
sub serveStatus {
 my ($client, $share) = @_;
 my ($buf, $req);
 while (sysread($client, $buf, 1) && $buf ne "\r") { #don't use buffered read or can hang if several strings arrive at once
	$req .= $buf; #build up the first line
 }
 unless ($buf eq "\r") {
 	print "\rWARNING: Status client provided incomplete request: ignoring.\n";
 	return;
 }
 unless ($req =~ m|^GET /(.*) HTTP/1.[01]|) {
 	print "\rWARNING: Status client sent bad HTTP request '$req': ignoring.\n";
	print $client "HTTP/1.0 400 BAD REQUEST\nContent-Type: text/plain\n\n";
	print $client "BAD REQUEST\n";
	return;
 }
 if (uc $1 eq 'AVAILABILITY') {
	print $client "HTTP/1.0 200 OK\nContent-Type: text/plain\n\n";
	print $client "{\"availability\" : true}\n";
 }
 elsif ($1 eq '') { #the data
	print $client "HTTP/1.0 200 OK\nContent-Type: text/plain\n\n";
	print $client "{\n";
	print $client "\t\"state\" : \"info\",\n"; # TODO expand this with "ok", "warn" or "error"
	print $client "\t\"monitorData\" : {\n";

	#don't use free-form strings as names in case they contain incompatible characters
	print $client "\t\t\"recordings\" : [\n";
	if (keys %events) {
		my @output;
		foreach (sort keys %events) { #there will always be at least one pair for each priority
			push @output, "\t\t\t{ \"name\" : " . E($_) . ', "number" : ' . keys(%{ $events{$_} }) . ' }';
		}
		print $client join(",\n", @output), "\n";
	}
	print $client "\t\t],\n";

	print $client "\t\t\"endpoints\" : {\n";
	my $transfers = thaw($share->fetch); #fetch is atomic so no need to lock as not changing $transfers
	if (keys %{ $transfers->{transfers} }) {
		my @priorities;
		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			my %pairs = %{ $transfers->{transfers}{$priority} };
			my @output;
			foreach (sort keys %pairs) { #there will always be at least one pair for each priority
				push @output, "\t\t\t\t{ \"source\" : " . E($_) . ', "destination" : ' . E($pairs{$_}{dest}) . ' }';
			}
			push @priorities, "\t\t\t$priority : [\n" . (join ",\n", @output) . "\n\t\t\t]";
		}
		print $client join(",\n", @priorities), "\n";
	}
	print $client "\t\t},\n";

	print $client "\t\t\"current\" : {\n";
	if ($transfers->{current}) { #at least one copy operation has started
		my @output;
		push @output, "\t\t\t\"priority\" : $transfers->{current}{priority}";
		push @output, "\t\t\t\"totalFiles\" : $transfers->{current}{totalFiles}";
		push @output, "\t\t\t\"totalSize\" : $transfers->{current}{totalSize}";
		if ($transfers->{current}{fileName}) { #at least one copy operation has started
			push @output, "\t\t\t\"fileName\" : " . E($transfers->{current}{fileName});
			push @output, "\t\t\t\"fileSize\" : $transfers->{current}{fileSize}";
		}
		print $client join(",\n", @output), "\n";
	}
	print $client "\t\t},\n";

	print $client "\t\t\"progress\" : {\n";
		if ($transfers->{current}{fileName}) { #at least one copy operation has started
			print $client "\t\t\t\"fileProgress\" : $transfers->{current}{fileProgress}\n";
		}
	print $client "\t\t}\n";

	print $client "\t}\n";
	print $client "}\n";
 }
 else {
 	print "\rWARNING: Status client asked for unknown page '$1': ignoring.\n";
	print $client "HTTP/1.0 404 FILE NOT FOUND\n";
	print $client "Content-Type: text/plain\n\n";
	print $client "Unknown page\n";
 }
}

#escape backslash and quotes
sub E {
	my $string = shift;
	$string =~ s/\\/\\\\/g;
	$string =~ s/"/\\"/g;
	return "\"$string\"";
}

#save priorites, pairs and copy progress in a file for later resumption
#assumes share is locked and should be stored afterwards
sub SaveConfig { #should be locked before doing this to prevent concurrent file writes
	my $transfers = shift;
 	my $config = IO::File->new('>' . PATHS_FILE);
 	if ($config) {
 		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			print $config " $priority\n"; #space indicates priority value
			print $config "$transfers->{extraDest}\n" if 1 == $priority;
			my %sources = %{ $transfers->{transfers}{$priority} };
			foreach (sort keys %sources) {
				print $config "$_\n$sources{$_}{dest}\n$sources{$_}{ctimeCleared}\n";
				print $config "$sources{$_}{extraCtimeCleared}\n" if 1 == $priority;
			}
 		}
 		$config->close;
 	}
 	else {
 		Report("WARNING: couldn't open \"" . PATHS_FILE . "\" for writing: $!.\nTransfers will not recommence automatically if script is restarted.\n", 1, $transfers);
 	}
}

# add a source/dest directory pair to the transfers
# if $ctimeMode = 0 or 1, read ctimeCleared (and extraCtimeCleared if priority is 1) from @$args and if 1, clear it/them; if 2, read from existing pair or clear if not present
# @$args can be large and only the needed values will be shifted off it
# return 2 if OK, 1 if problem with dirs and 0 if problem with args
# assumes share is locked
sub addPair {
 #check args
 my ($transfers, $args, $priority, $clearExtraCtime, $ctimeMode) = @_;
 my $source = shift @$args;
 my $dest = shift @$args;
 unless ($priority =~ /^\d+$/) {
	print "\rWARNING: priority argument '$priority' is invalid\n";
	return 0;
 }
 unless (defined $source) {
	print "\rWARNING: source directory argument missing\n";
	return 0;
 }
 $source =~ s|/*$||;
 unless (defined $dest) {
	print "\rWARNING: destination directory argument missing\n";
	return 0;
 }
 $dest =~ s|/*$||;
 print "\rPriority $priority: '$source/*' -> '$dest/*'\n";
 my ($ctimeCleared, $extraCtimeCleared);
 unless (2 == $ctimeMode) {
	$ctimeCleared = shift @$args;
	unless (defined $ctimeCleared) {
		print "\rWARNING: no ctimeCleared value: ignoring this pair\n";
		return 0;
	}
	unless ($ctimeCleared =~ /^\d+$/) {
		print "\rWARNING: invalid ctimeCleared value '$ctimeCleared': ignoring this pair\n";
		return 0;
	}
	if (1 == $priority) {
		$extraCtimeCleared = shift @$args;
		unless (defined $extraCtimeCleared) {
			print "\rWARNING: no extraCtimeCleared value: ignoring this pair\n";
			return 0;
		}
		unless ($extraCtimeCleared =~ /^\d+$/) {
			print "\rWARNING: invalid extraCtimeCleared value '$extraCtimeCleared': ignoring this pair\n";
			return 0;
		}
	}
 }
 $ctimeCleared = 0 if 1 == $ctimeMode;
 $extraCtimeCleared = 0 if 1 == $ctimeMode || $clearExtraCtime;
 if (!-d $source) {
	print "\rWARNING: source directory '$source/' doesn't exist: ignoring\n";
	return 1;
 }
 print "Also copying to extra destination '$opts{e}/'\n" if 1 == $priority && $opts{e};
 #store details
 foreach (keys %{$transfers->{transfers}}) {
	if (exists $transfers->{transfers}{$_}{$source}) {
		$ctimeCleared = $transfers->{transfers}{$_}{$source}{ctimeCleared} unless defined $ctimeCleared; #if this is a different priority then the ctimeCleared value is still valid
		$extraCtimeCleared = $transfers->{transfers}{$_}{$source}{extraCtimeCleared} if exists $transfers->{transfers}{$_}{$source}{extraCtimeCleared} && !defined $extraCtimeCleared; #if this is a different priority then the ctimeCleared value is still valid
		delete $transfers->{transfers}{$_}{$source}; #may be putting this straight back again!
		last; #shouldn't be more than one!
	}
 }
 $transfers->{transfers}{$priority}{$source}{dest} = $dest; #using $source as a hash key prevents duplication of sources
 $transfers->{extraDest} = ($opts{e} ? $opts{e} : '') if 1 == $priority;
 $transfers->{transfers}{$priority}{$source}{stale} = STALE;
 if (defined $ctimeCleared) {
 	$transfers->{transfers}{$priority}{$source}{ctimeCleared} = $ctimeCleared;
 	$transfers->{transfers}{$priority}{$source}{extraCtimeCleared} = $extraCtimeCleared if 1 == $priority;
 }
 else {
 	$transfers->{transfers}{$priority}{$source}{ctimeCleared} = 0; #copy everything from the epoch onwards
 	$transfers->{transfers}{$priority}{$source}{extraCtimeCleared} = 0 if 1 == $priority;
 }
 #tell the copying thread
 if (!exists $transfers->{staleFrom}) { #not pending rescan
	print CHILD "\n"; #will kick the child into action if it's idle
 }
 if (!exists $transfers->{staleFrom} || $transfers->{staleFrom} > $priority) { #not pending rescan or re-scanning from a lower priority than this pair
	$transfers->{staleFrom} = $priority; #rescan from this priority at the end of the current copy
 }
 return 2;
}

# scan a source/destination directory pair, make a list of files that need to be copied, and delete unneeded source files
sub Scan {
 my ($handle, $srcDir, $destRoot, $ctimeCleared, $share, $extraDestRoot, $extraCtimeCleared, $ftpFileList) = @_;
 my ($totalFiles, $totalToCopy, $extraTotalToCopy, $totalSize, $extraTotalSize, $latestCtime) = (0, 0, 0, 0, 0, 0);
 my (@essenceFiles, @extraEssenceFiles);
 my ($ok, $extraOk) = (1, defined $extraDestRoot);
 my @files = sort readdir $handle; #sorting useful for verbose option
 while (@files && ($ok || $extraOk)) {
	my $srcName = shift @files;
	my $fullSource = "$srcDir/$srcName";
	if (-d $fullSource) {
		Report("d  $srcName\n", 0, $share) if $opts{v} && $srcName !~ /^\.\.?$/;
		next;
	}
	#generate temp dest path
	my ($destDir, $extraDestDir);
	if ($opts{d}) { #don't use subdirectories
		if ($srcName =~ /\.[mM][xX][fF]$/ || $srcName =~ /\.[mM][oO][vV]$/) { #NB see subst below before changing this
			$destDir = $destRoot;
			$extraDestDir = $extraDestRoot if defined $extraDestRoot;
		}
		else {
			Report("- $srcName\n", 0, $share) if $opts{v} && $srcName !~ /^\.\.?$/;
			next;
		}
	}
	else {
		if ($srcName =~ /^(\d{8})_.*\.[mM][xX][fF]$/ || $srcName =~ /^(\d{8})_.*\.[mM][oO][vV]$/) { #NB see subst below before changing this
			$destDir = "$destRoot/$1";
			$extraDestDir = "$extraDestRoot/$1" if defined $extraDestRoot;
		}
		else {
			Report("-  $srcName\n", 0, $share) if $opts{v} && $srcName !~ /^\.\.?$/;
			next;
		}
	}
	$totalFiles++;
	#get info about the file
	#Use ctime to test for files to be ignored that have been copied already.
	#This is because ctime is updated when the completed files are atomically moved to the source directories,
	#so we can guarantee that files with an earlier ctime won't appear after we've copied everything up to a certain ctime,
	#which could otherwise result in files not being copied.
	#Use mtime to test for deleting old files, because age of file since creation/modification is what is important.
	my ($dev, $size, $mtime, $ctime) = (stat $fullSource)[0, 7, 9, 10];
	unless ($dev) { #if lstat fails then it returns a null list
		Report("WARNING: failed to stat '$fullSource': $!: abandoning this directory for this copying cycle\n", 0, $share);
		$ok = 0;
		$extraOk = 0;
		last;
	}
	unless ($size) {
		Report("WARNING: '$fullSource' has zero length: ignoring\n", 0, $share); #if the file later acquires length, its ctime will be updated, so useful content won't be deleted without copying
		next;
	}
	$latestCtime = $ctime if $ctime > $latestCtime;
	#check for existing copies and generate copy details
	(my $altName = $srcName) =~ s/(.*)(.{4})/$1_dup$2/; #will always succeed due to check above
	my $copied = $ctime <= $ctimeCleared; #already copied if ctime indicates it
	my @checkDest = CheckDest($srcDir, $destDir, $srcName, $size, $altName, $share, $ftpFileList); #even if this dir has been abandoned, still check for a copy to see if we can delete the source file
	$ok &= $checkDest[0];
	Report(($copied ? 'c' : ' ') . ($checkDest[1] ? 's' : ' ') . " $srcName\n", 0, $share) if $opts{v};
	$copied |= $checkDest[1];
	if ($ok & !$copied) {
		push @essenceFiles, {
			name => $srcName,
			srcDir => $srcDir,
			destDir => $destDir,
			mtime => $mtime,
			ctime => $ctime,
			size => $size,
			extra => 0,
		};
		$totalToCopy++;
		$totalSize += $size;
	}
	my $extraCopied = 1; #default if no extra destination
	if (defined $extraDestDir) {
		$extraCopied = $ctime <= $extraCtimeCleared; #already copied if ctime indicates it
		@checkDest = CheckDest($srcDir, $extraDestDir, $srcName, $size, $altName, $share); #never use ftp; even if this dir has been abandoned, still check for a copy to see if we can delete the source file
		$extraOk &= $checkDest[0];
		Report(($extraCopied ? 'c' : ' ') . ($checkDest[1] ? 's' : ' ') . " $srcName (extra)\n", 0, $share) if $opts{v};
		$extraCopied |= $checkDest[1];
		if ($extraOk & !$extraCopied) {
			push @extraEssenceFiles, {
				name => $srcName,
				srcDir => $srcDir,
				destDir => $extraDestDir,
				mtime => $mtime,
				ctime => $ctime,
				size => $size,
				extra => 1,
			};
			$extraTotalToCopy++;
			$extraTotalSize += $size;
		}
	}
	#delete source file if redundant
	if ($copied
	 && $extraCopied
	 && time > $mtime + KEEP_PERIOD
	 && dfportable($srcDir)->{per} >= DELETE_THRESHOLD) {
		if ($opts{p}) {
			Report("Would delete old file '$fullSource' if not prevented by command-line option, as aleady copied and disk is " . dfportable($srcDir)->{per} . "% full.\n", 0, $share);
		}
		else {
			Report("Removing old file '$fullSource', as already copied and disk is " . dfportable($srcDir)->{per} . "% full.\n", 0, $share);
			unlink $fullSource or Report("WARNING: could not remove '$fullSource': $!\n", 0, $share);
		}
	}
 }
 unless ($ok) {
 	@essenceFiles = ();
	$totalToCopy = 0;
	$totalSize = 0;
 }
 unless ($extraOk) {
 	@extraEssenceFiles = ();
	$extraTotalToCopy = 0;
	$extraTotalSize = 0;
 }
 if ($ok || $extraOk) {
	if ($totalFiles) {
		Report("Directory contains $totalFiles recognised file" . (1 == $totalFiles ? '' : 's') . ".\n", 0, $share);
		Report("$totalToCopy will be copied ($totalSize bytes) to '$destRoot/'.\n", 0, $share) if $ok;
		Report("$extraTotalToCopy will be copied ($extraTotalSize bytes) to extra destination '$extraDestRoot/'.\n", 0, $share) if $extraOk;
	}
	else {
		Report("Directory contains no recognised files.\n", 0, $share);
	}
 }
 return $ok, $extraOk, $latestCtime, $totalToCopy, $extraTotalToCopy, $totalSize + $extraTotalSize, @essenceFiles, @extraEssenceFiles; #arrays will be concatenated
}

#checks to see if a destination file already exists; if so and it's the same size, indicate that it's already copied; if not the same size, try to move it out of the way by renaming
sub CheckDest
{
 my ($srcDir, $destDir, $name, $size, $altName, $share, $ftpFileList) = @_;
 my $fullDest = "$destDir/$name";
 my $copied = 0;
 if ($opts{f} && $ftpFileList) {
	my $url = "ftp://$ftpDetails[0]/" . (FTP_ROOT ne '' ? FTP_ROOT . '/' : '') . "$destDir/";
	if (!exists $$ftpFileList{$destDir}) { #haven't got a listing of this directory yet
		#use curl to get a directory listing.  Need stderr to get messages in case it fails, but disable the progress meter as it puts stuff in the middle of listing lines.
		my $cmd = "$curl -s -S --ftp-create-dirs $url 2>&1"; #allow creation of dirs to prevent an error if directory doesn't exist; FIXME: doesn't detect if it fails to open the directory
		my @listing = `$cmd`;
		if ($?) {
			Report("WARNING: couldn't get listing of '$url':\n@{listing}Abandoning this directory\n", 0, $share); #$! doesn't work
			return 0, 0;
		}
		foreach (@listing) {
			next unless (/^-(\S+\s+){4}(\d+).*?(\S+)$/); #ignore directories and messages from stderr; this is compatible with a vsftpd listing
			$$ftpFileList{$destDir}{$3} = $2; #filename -> size
		}
	}
	if (exists $$ftpFileList{$destDir}{$name}) {
		if ($$ftpFileList{$destDir}{$name} == $size) {
			$copied = 1;
		}
		else {
			Report("WARNING: '$fullDest' exists and is a different size ($$ftpFileList{$destDir}{$name}) from the same file in '$srcDir' ($size): may be a filename conflict: renaming to '$altName'.\n", 0, $share);
			if (exists $$ftpFileList{$destDir}{$altName} && $$ftpFileList{$destDir}{$altName}) {
				Report("WARNING: '$altName' already exists: abandoning this directory\n", 0, $share);
				return 0, 0;
			}
			else {
				if (system "$curl --quote '-RNFR $name' --quote '-RNTO $altName' $url 2>/dev/null") { # FIXME: can't handle spaces in paths
					Report("WARNING: Failed to rename file in '$url': $!: abandoning this directory\n", 0, $share);
					return 0, 0;
				}
				#now the entry in %$ftpFileList for this file is wrong, but doesn't matter as it won't be looked at again
			}
		}
	}
 }
 elsif (-s $fullDest) {
	if (-s $fullDest == $size) {
		$copied = 1;
	}
	else {
		Report("WARNING: '$fullDest' exists and is a different size (" . (-s $fullDest) . ") from the same file in '$srcDir' ($size): may be a filename conflict: renaming to '$altName'.\n", 0, $share);
		if (-s "$destDir/$altName") {
			Report("WARNING: '$altName' already exists: abandoning this directory\n", 0, $share);
			return 0, 0;
		}
		elsif (!rename $fullDest, "$destDir/$altName") {
			Report("WARNING: Failed to rename file: $!: abandoning this directory\n", 0, $share);
			return 0, 0;
		}
	}
 }
 return 1, $copied;
}

sub childLoop {
 my $share = shift;
 $SIG{INT} = sub { exit; };
 $SIG{TERM} = sub { exit; };
 $SIG{USR1} = 'IGNORE';
 $SIG{USR2} = 'IGNORE';
 local $| = 1; #autoflush stdout

 my $retry = 0;
 while (1) {
	#wait for main thread or retry interval
	my $rin = "";
	vec($rin, fileno(STDIN), 1) = 1;
	if (!select $rin, undef, $rin, $retry ? RECHECK_INTERVAL : undef) { #timed out
		Report("Retrying...\n", 0, $share);
		$share->lock(LOCK_EX);
	}
	else { #kicked by the main thread
		$share->lock(LOCK_EX) unless $terminate; #do this now to keep STDIN in sync with st$totalFilesaleFrom to avoid unnecessary scans; condition prevents a hang on interrupt
		<STDIN>;
	}
	my $transfers = thaw($share->fetch);
	delete $transfers->{staleFrom}; #checking started
	my @priorities = sort keys %{ $transfers->{transfers} };
	my $priority;
	#change all stale source dirs to PROCESSING so we can determine where new files arrive while copying
	foreach $priority (@priorities) {
		foreach (keys %{ $transfers->{transfers}{$priority} }) {
			$transfers->{transfers}{$priority}{$_}{stale} = PROCESSING unless UP_TO_DATE == $transfers->{transfers}{$priority}{$_}{stale};
		}
	}
	$retry = 0;
	my $interrupting = 0;
	my %incomingDirs; #paths of temporary incoming directories, so they can be deleted on completion
	my %ftpFileList; #file names and sizes on server if using FTP, to avoid repeated FTP calls
	Report("Verbose mode. c: ctime cleared; s: on server; d: directory; -: unrecognised\n", 1, $transfers) if $opts{v};
	foreach my $priority (@priorities) { #NB share locked at this point
		last if $interrupting;
		$share->store(freeze $transfers);
		$share->unlock;
		my %pairs = %{ $transfers->{transfers}{$priority} };
		my @essenceFiles;
		my ($totalFiles, $totalSize) = (0, 0);
		#make a list of all the files to copy at this priority
		foreach (sort keys %pairs) {
			next unless PROCESSING == $pairs{$_}{stale}; #no point in scanning if not stale

			#generate an array of files that need to be copied
			if (!opendir(SRC, $_)) {
				Report("WARNING: failed to open source directory '$_/': $!\n", 0, $share);
				$retry = 1;
				next;
			}
			my @scan;
			my $scanTime;
			my $extra = 1 == $priority && '' ne $transfers->{extraDest};
			my $loop;
			do { #loop to make sure we get all the files with the latest ctime we find
				$scanTime = time;
				Report("Priority $priority: scanning '$_/'...\n", 0, $share);
				@scan = Scan(*SRC, $_, $pairs{$_}{dest}, $pairs{$_}{ctimeCleared}, $share, $extra ? $transfers->{extraDest} : undef, $pairs{$_}{extraCtimeCleared}, \%ftpFileList); #enable extra destination if at priority 1 and it is supplied
				$loop = ($scan[0] || $scan[1]) && $scan[2] >= $scanTime; #if we find files with the same or later ctime as when (just before) the scan started, scan again in case more files have appeared with the same ctime as the latest found
				if ($loop) {
					rewinddir SRC;
					sleep 1; #delay the retry to prevent a possible deluge of scans
				}
			} while $loop;
			closedir(SRC);
			$retry |= !$scan[0] || !($scan[1] || !$extra);
			#update stale status and saved ctimeCleared values if they won't be updated anyway
			#- this prevents scans of up-to-date directories
			my $locked = 0;
			if ($scan[0] && ($scan[1] || !$extra) && !$scan[5]) { #up to date
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$locked = 1;
				if (exists $transfers->{transfers}{$priority}{$_} #make sure config hasn't changed
				 && PROCESSING == $transfers->{transfers}{$priority}{$_}{stale}) { #new files haven't appeared since scanning
					$transfers->{transfers}{$priority}{$_}{stale} = UP_TO_DATE;
				}
			}
			my $needToSave = 0;
			if (shift @scan && $scan[1] != $pairs{$_}{ctimeCleared} && !$scan[2]) { #not copying anything from this source to main dest so ctimeCleared won't get updated, but has changed
				#save the new ctimeCleared value
				unless ($locked) {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					$locked = 1;
				}
				if (exists $transfers->{transfers}{$priority}{$_}) { #make sure config hasn't changed
					$transfers->{transfers}{$priority}{$_}{ctimeCleared} = $scan[1];
				}
				$needToSave = 1;
			}
			if (shift @scan && $scan[0] != $pairs{$_}{extraCtimeCleared} && !$scan[2]) { #not copying anything from this source to extra dest so extraCtimeCleared won't get updated, but has changed
				#save the new ctimeCleared value
				unless ($locked) {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					$locked = 1;
				}
				if (exists $transfers->{transfers}{$priority}{$_}) { #make sure config hasn't changed
					$transfers->{transfers}{$priority}{$_}{extraCtimeCleared} = $scan[0];
				}
				$needToSave = 1;
			}
			if ($locked) {
				SaveConfig($transfers) if $needToSave;
				$share->store(freeze $transfers);
				$share->unlock;
			}

			#add information to copying details
 			$pairs{$_}{latestCtime} = shift @scan; #for updating saved value correctly even if youngest file doesn't need to be copied
 			$pairs{$_}{nFiles} = shift @scan; #so we can determine when all files in this directory have been copied
 			$pairs{$_}{extraNFiles} = shift @scan; #so we can determine when all files in this directory have been copied (if used)
			$totalSize += shift @scan;
			push @essenceFiles, @scan; #takes all remaining values, i.e. both arrays
			$totalFiles += $pairs{$_}{nFiles} + $pairs{$_}{extraNFiles};
		}

		#update stats
		$share->lock(LOCK_EX);
		$transfers = thaw($share->fetch);
		$transfers->{current}{priority} = $priority;
		$transfers->{current}{totalFiles} = $totalFiles;
		$transfers->{current}{totalSize} = $totalSize;
		my $prevFile;

		#copy the files at this priority, oldest first
		foreach my $essenceFile (sort { $a->{ctime} <=> $b->{ctime} } @essenceFiles) { #NB share locked at this point
			if (exists $transfers->{staleFrom} && $transfers->{staleFrom} < $priority) {
				#retry as new files of a higher priority may have appeared
				$interrupting = 1;
				last;
			}
			my $latestCtimeCopied = $essenceFile->{extra} ? 'extraLatestCtimeCopied' : 'latestCtimeCopied';
			my $ctimeCleared = $essenceFile->{extra} ? 'extraCtimeCleared' : 'ctimeCleared';
			if (exists $pairs{$essenceFile->{srcDir}}{$latestCtimeCopied}) { #have already tried to copy something from this source dir
				if (-1 == $pairs{$essenceFile->{srcDir}}{$latestCtimeCopied}) { #have had a problem with this source dir
					next; #ignore this source dir
				}
				elsif ($essenceFile->{ctime} > $pairs{$essenceFile->{srcDir}}{$latestCtimeCopied}) { #this file is newer than all files so far copied so all the files up to the age of the last one copied inclusive must have been copied
					$pairs{$essenceFile->{srcDir}}{$ctimeCleared} = $pairs{$essenceFile->{srcDir}}{$latestCtimeCopied};
					#update the saved configuration so if the script is stopped now it won't re-copy files which have disappeared from the server when restarted
					if (exists $transfers->{transfers}{$priority}{$essenceFile->{srcDir}}{$ctimeCleared}) { #in case this directory pair has been moved to another priority
						$transfers->{transfers}{$priority}{$essenceFile->{srcDir}}{$ctimeCleared} = $pairs{$essenceFile->{srcDir}}{$latestCtimeCopied};
						SaveConfig($transfers);
					}
				}
			}
			my $incomingPath = "$essenceFile->{destDir}/" . DEST_INCOMING;
			if (!$opts{f} || $essenceFile->{extra}) { #when using ftp, the transfer command creates the directory and there is no way to find out how much free space there is
				# check/create copy subdir
				if (!-d $incomingPath && MakeDir($incomingPath)) {
					Report("WARNING: Couldn't create '$incomingPath': $!\n", 1, $transfers);
					$pairs{$essenceFile->{srcDir}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source directory/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
				# check free space
				if ((my $free = dfportable($essenceFile->{destDir})->{bavail}) < $essenceFile->{size}) { #a crude test but which will catch most instances of insufficient space.  Assumes subdirs are all on the same partition
					Report("Prio $priority: '$essenceFile->{destDir}' full ($free byte" . ($free == 1 ? '' : 's') . " free; file size $essenceFile->{size} bytes).\nAbandoning copying from '$essenceFile->{srcDir}/'\n", 1, $transfers);
					$pairs{$essenceFile->{srcDir}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source directory/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
				$incomingDirs{dir}{$incomingPath} = 1; #so they can be deleted on completion
			}
			else {
				$incomingDirs{ftp}{$essenceFile->{destDir}} = DEST_INCOMING; #so they can be deleted on completion
			}
			#update stats
			$transfers->{current}{fileName} = "$essenceFile->{srcDir}/$essenceFile->{name}";
			$transfers->{current}{fileSize} = $essenceFile->{size};
			$transfers->{current}{fileProgress} = 0; #in case a status request is made now
			# copy the file
			$share->store(freeze $transfers);
			$share->unlock;
			my $childMsgs;
			my $cpPid;
			if (!$opts{f} || $essenceFile->{extra}) { # block limits scope of signal handlers
				local $SIG{INT} = sub { kill 'INT', $cpPid; };
				local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
				# (doing the above with the USR signals doesn't seem to work properly when using a forking open rather than a fork)
				# run copy program with a forking open so that we can get its stdout
				Report("Prio $priority ($transfers->{current}{totalFiles} to go): '$essenceFile->{srcDir}/$essenceFile->{name}' -> '$incomingPath/':\n", 1, $transfers);
				if (!defined ($cpPid = open(COPYCHILD, '-|'))) {
					die "failed to fork: $!";
				}
				elsif ($cpPid == 0) { # child
					STDOUT->autoflush(1);
					# exec to maintain PID so we can send signals to the copy program
					exec COPY,
						1, # always start bandwidth-limited because traffic control might be switched on between now and when the program starts, causing it to miss the signal
						FAST_LIMIT, # the bandwidth limit at high speed
						SLOW_LIMIT, # the bandwidth limit at slow speed
						"$essenceFile->{srcDir}/$essenceFile->{name}", #source file
						"$incomingPath/$essenceFile->{name}" #dest file
					or print "failed to exec copy command: $!";
					exit 1;
				}
				STDOUT->autoflush(1);
				# note the PID so that the foreground process can signal the copy program
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$transfers->{pid} = $cpPid;
				$share->store(freeze $transfers);
				$share->unlock;
				# get progress from the copy while waiting for it to finish
				my ($buf, $progress, $line);
				my $signalled = 0;
				while (sysread(COPYCHILD, $buf, 100)) { # can't use <> because there aren't any intermediate newlines
					if (!$signalled) { # first time the copy prog has printed anything so it's just come alive
						$signalled = 1; # only need to do this once
						kill 'USR2', $cpPid unless $transfers->{limit}; # switch to fast copying
					}
					$line .= $buf;
					$line =~ s/^(.*?)([\r\n]?)$/$2/s; #remove everything up to any trailing return/newline as we might want to overwrite this line
					Report($1, 0, $share, 1) if !$?; #don't print error messages twice
					$childMsgs .= $1;
					$childMsgs =~ s/.*[\r\n]//s; #remove anything before last line
					$progress .= $buf;
					if ($progress =~ /(\d+)%/) { # % complete figure available
						#update stats
						$share->lock(LOCK_EX);
						$transfers = thaw($share->fetch);
						$transfers->{current}{fileProgress} = $1;
						$share->store(freeze $transfers);
						$share->unlock;
						$progress = ''; #assume not more than one will be present
					}
				}
				# prevent signalling (this doesn't suffer from a race condition)
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				$transfers->{pid} = 0;
				$share->store(freeze $transfers);
				$share->unlock;
				close COPYCHILD;
			}
			else { #ftp transfer
				local $SIG{INT} = sub { kill 'INT', $cpPid; };
				local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
				my $url = "ftp://$ftpDetails[0]/" . (FTP_ROOT ne '' ? FTP_ROOT . '/' : '') . "$incomingPath/";
				Report("Prio $priority ($transfers->{current}{totalFiles} to go): '$essenceFile->{srcDir}/$essenceFile->{name}' -> '$url':\n", 1, $transfers);
				# run copy program with a forking open so that we can get its stdout
				if (!defined ($cpPid = open COPYCHILD, '-|')) {
					die "failed to fork: $!";
				}
				elsif ($cpPid == 0) { # child
					STDOUT->autoflush(1);
					# exec to maintain PID so we can send signals to the copy program
					exec "$curl" . #use shell because curl sends progress bar to stderr so need to redirect
						" --ftp-create-dirs" .
						" --upload-file $essenceFile->{srcDir}/$essenceFile->{name}" .
						" --quote '-RNFR $essenceFile->{name}' --quote '-RNTO ../$essenceFile->{name}'" . #move out of incoming dir after copying
						" $url" .
						' 2>&1' #do not remove DEST_INCOMING here or whole operation will fail if it can't remove the dir (eg if it's not empty due to another copying process)
					or print "failed to exec ftp command: $!";
					exit 1;
				}
				STDOUT->autoflush(1);
				# get progress from the copy while waiting for it to finish
				my ($buf, $progress, $line);
				while (sysread(COPYCHILD, $buf, 100)) { # can't use <> because there aren't any intermediate newlines
					$line .= $buf;
					$line =~ s/^(.*?)([\r\n]?)$/$2/s; #remove everything up to any trailing return/newline as we might want to overwrite this line
					Report($1, 0, $share, 1) if !$?; #don't print error messages twice
					$childMsgs .= $1;
					$childMsgs =~ s/.*[\r\n]//s; #remove anything before last line
					$progress .= $buf;
					if ($progress =~ /\r\s*(\d+)\s/s) { # % complete figure available
						#update stats
						$share->lock(LOCK_EX);
						$transfers = thaw($share->fetch);
						$transfers->{current}{fileProgress} = $1;
						$share->store(freeze $transfers);
						$share->unlock;
						$progress = ''; #assume not more than one will be present
					}
				}
				close COPYCHILD;
			}
			# check result
			if ($? & 127) { #child has received terminate signal so terminate
				unlink "$incomingPath/" . $essenceFile->{name};
				exit 1; # TODO: use more exit code values
			}
			if ($? >> 8) { #copy failed
				unlink "$incomingPath/$essenceFile->{name}" if !$opts{f} || $essenceFile->{extra};
				$share->lock(LOCK_EX);
				$transfers = thaw($share->fetch);
				Report("WARNING: copy failed, exiting with value " . ($? >> 8) . ": $childMsgs\n", 1, $transfers);
				$pairs{$essenceFile->{srcDir}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source directory/latestCtimeCopied, which would break the ctime order rule
				$retry = 1;
				next;
			}
			# move the file from the incoming directory to the live directory
			if (!$opts{f} || $essenceFile->{extra}) {
				$childMsgs =~ s/ +$//; #remove the trailing spaces put in by cpfs to hide the "Flushing buffers..." message
				Report("\r$childMsgs. Moving...", 0, $share, 1);
				if (!rename "$incomingPath/$essenceFile->{name}", "$essenceFile->{destDir}/$essenceFile->{name}") {
					$share->lock(LOCK_EX);
					$transfers = thaw($share->fetch);
					Report("\nWARNING: Failed to move file to '$essenceFile->{destDir}/': $!\nAbandoning copying from '$essenceFile->{srcDir}/'\n", 1, $transfers);
					$pairs{$essenceFile->{srcDir}}{$latestCtimeCopied} = -1; #prevents any more transfers from this source directory/latestCtimeCopied, which would break the ctime order rule
					$retry = 1;
					next;
				}
			}
			Report(" Done\n", 0, $share, 1);
			$pairs{$essenceFile->{srcDir}}{$latestCtimeCopied} = $essenceFile->{ctime};
			# update stats
			$share->lock(LOCK_EX);
			$transfers = thaw($share->fetch);
			$transfers->{current}{totalFiles}--;
			$transfers->{current}{totalSize} -= $essenceFile->{size};
			if (!--$pairs{$essenceFile->{srcDir}}{$essenceFile->{extra} ? 'extraNFiles' : 'nFiles'}) { #this set all copied
				Report("All files from '$essenceFile->{srcDir}/' to '$essenceFile->{destDir}/' successfully copied.\n", 1, $transfers);
				if (!exists $pairs{$essenceFile->{srcDir}}{$essenceFile->{extra} ? 'nFiles' : 'extraNFiles'} || !$pairs{$essenceFile->{srcDir}}{$essenceFile->{extra} ? 'nFiles' : 'extraNFiles'}) { #other set not present or all copied
					#mark this source dir as up to date
					if (exists $transfers->{transfers}{$priority}{$essenceFile->{srcDir}}) { #pair hasn't disappeared
						if (PROCESSING == $transfers->{transfers}{$priority}{$essenceFile->{srcDir}}{stale}) { #new files haven't appeared since scanning
							$transfers->{transfers}{$priority}{$essenceFile->{srcDir}}{stale} = UP_TO_DATE;
						}
						$transfers->{transfers}{$priority}{$essenceFile->{srcDir}}{$ctimeCleared} = $pairs{$essenceFile->{srcDir}}{latestCtime}; #use {latestCtime} rather than the ctime of the current file to ensure that the saved ctimeCleared reflects the age of the youngest scanned file even if this file has not been copied this time (ctimeCleared values in paths file reset and old but not newer files have been deleted off the server?)
						SaveConfig($transfers);
					}
					else {
						Report("Configuration has changed: rescanning\n", 1, $transfers);
						@priorities = ();
						last;
					}
				}
			}
		} #file copy loop
	} # priorities loop
	delete $transfers->{current};
	$share->store(freeze $transfers);
	$share->unlock;
	#remove temp subdirs (in all cases because config might have changed)
	foreach (keys %{ $incomingDirs{dir} }) {
		rmdir;
	}
	foreach (keys %{ $incomingDirs{ftp} }) {
		my $url = "ftp://$ftpDetails[0]/" . (FTP_ROOT ne '' ? FTP_ROOT . '/' : '') . "$_/$incomingDirs{ftp}{$_}/";
		Report("WARNING: failed to remove directory '$incomingDirs{ftp}{$_}/' from '$_' in '$url' (may be another upload to this directory in progress)\n", 0, $share) if 21 << 8 == system "$curl" .
			" --quote '-CWD ..' --quote '-RMD $incomingDirs{ftp}{$_}/'" . #will fail at this point with rc of 21 if directory not empty
			" $url" . #will fail at this point with rc of 9 if directory not present (which doesn't matter - another copying process has removed it in the mean time)
			' 2>/dev/null';
	}
	if ($interrupting) {
		Report("Interrupting copying to check for new files of a higher priority\n", 0, $share);
	}
	elsif ($retry) {
		Report('Pausing for ' . RECHECK_INTERVAL . " seconds before retrying.\n\n", 0, $share);
	}
	elsif (!exists $transfers->{staleFrom}) {
		Report("Idle.\n\n", 0, $share);
	}
 }
}

sub Abort {
 $terminate = 1;
 kill "INT", $childPid if defined $childPid;
 waitpid($childPid, 0) if defined $childPid; #so that it's safe to remove the shared object
 my $msg = shift;
 $msg |= '';
 FtpTrafficControl(0) if $opts{f};
 die "\nAborting: $msg\n";
}

sub MakeDir { #makes a directory (recursively) without the permissions being modified by the current umask
	return system 'mkdir', '-p', '-m', PERMISSIONS, shift;
}

#mode == 0: $object is the unlocked share; mode == 1: share is already locked and object is the transfer hash (which should be saved later)
#notimestamp defined: don't print date or time and don't print newline if not at start of line
sub Report {
 my ($msg, $mode, $object, $notimestamp) = @_;
 my $transfers;
 if ($mode) { #transfers object supplied
	$transfers = $object;
 }
 else { #unlocked share supplied
	$object->lock(LOCK_EX);
	$transfers = thaw($share->fetch);
 }
 my $update = 0; #only freeze and store if we need to for efficiency
 if (defined $notimestamp) {
	print $msg;
 }
 else {
	my @now = localtime();
	my $newdate = sprintf '%d%02d%02d', $now[5] + 1900, $now[4] +1, $now[3];
	if ($newdate ne $transfers->{date}) {
		print "Date is $newdate.\n";
		$transfers->{date} = $newdate;
		$update = 1;
	}
	printf "%s%02d:%02d:%02d $msg", $transfers->{startOfLine} eq '1' ? '' : "\n", (@now)[2, 1, 0];
 }
 if ($transfers->{startOfLine} ne $msg =~ /\n$/s) {
	$transfers->{startOfLine} = $msg =~ /\n$/s;
	$update = 1;
 }
 $object->store(freeze $transfers) if $update && !$mode;
 $object->unlock unless $mode;
}

sub FtpTrafficControl { #Non-zero in first parameter switches traffic control on. Returns an empty string on success
 #get the first hop of a traceroute to the server
 my $cmd = "/usr/sbin/traceroute -m 1 $ftpDetails[0] 2>&1";
 my $hop = `$cmd`;
 return "Failed to run '$cmd': $!" if -1 == $?;
 return "traceroute failed: $hop" if $?;
 my @hop = $hop =~ /\n.*[ (](\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})[) ]/s; #extract the address of the first hop
 if (!@hop) {
	chomp $hop;
	return "Failed to extract hop address from traceroute output '$hop'";
 }
 #get the routing table
 $cmd = '/sbin/route 2>&1';
 my $table = `$cmd`;
 return "Failed to run '$cmd': $!" if -1 == $?;
 return "route failed: $table" if $?;
 #try to match each route (in order) to determine the interface in use
 my $interface;
 foreach my $entry (split /\n/, $table) {
	my @route = $entry =~ /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\s+\S+\s+(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}).+\s(\S+)$/;
	if (@route) {
		#mask hop address with network mask to convert to a network address, and compare with route destination
		foreach (0..3) {
			$hop[$_] += 0; #convert string to number so it does a numerical bitwise AND
			if (($hop[$_] & $route[$_ + 4]) != $route[$_]) { #doesn't match
				undef $route[8]; #the device name
				last;
			}
		}
		if (defined $route[8]) { #matched
			$interface = $route[8];
			last;
		}
	}
 }
 return "Failed to ascertain interface for FTP transfers to $ftpDetails[0] from routing table:\n$table" unless defined $interface;
 if ($_[0]) { #traffice control on
	$cmd = "sudo /usr/sbin/tc\\
		qdisc add\\
		dev $interface\\
		root\\
		handle 1:0\\
		htb\\
		2>&1 &&\\
		sudo /usr/sbin/tc\\
		class add\\
		dev $interface\\
		parent 1:0\\
		classid 1:1\\
		htb\\
		burst 0mbit\\
		cburst " . FTP_SPEED . "mbit\\
		rate " . FTP_SPEED . "mbit\\
		ceil " . FTP_SPEED . "mbit\\
		2>&1 &&\\
		sudo /usr/sbin/iptables\\
		--table mangle\\
		--append POSTROUTING\\
		--out-interface $interface\\
		--protocol tcp\\
		--destination $ftpDetails[0]\\
		--jump CLASSIFY --set-class 1:1\\
		2>&1";
	return `$cmd`;
 }
 else {
	return `sudo /usr/sbin/tc qdisc del dev $interface root 2>&1`;
 }
}
