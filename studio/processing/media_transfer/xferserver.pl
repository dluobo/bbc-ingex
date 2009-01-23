#! /usr/bin/perl -w

#/***************************************************************************
# *   Copyright (C) 2008 British Broadcasting Corporation                   *
# *   - all rights reserved.                                                *
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

# In the following description, the capitalised words refer to constants defined below.

# The script listens on network port CLIENT_PORT for clients to open connections, send parameters and then close connections.
# Parameters are as follows:
# <recorder name> <recording identifier> [<source path> <destination path>] ... (each terminated with a newline)
# The recorder name and recording identifier combination identify a recording, and must be unique so that the script can determine unambiguously if any recordings are taking place, even when a recorder starts a recording before finishing the previous one (due to postroll).
# If no other parameters are supplied, this indicates the start of a recording.  The recording's identifier is added to a recording list, and traffic control is switched on at rate SLOW_LIMIT.
# If additional pair(s) of paths are supplied, this indicates the end of a recording, or a recorder being started.  This will result in the recording's identifier being removed from the recording list (if present), and if the list is empty, traffic control is switched off (reverting to copying at FAST_LIMIT, which is unlimited if 0).
# Recording identifier values are arbitrary (most likely an incrementing number) except for the special case '0' (zero), which is interpreted as covering all recordings being made by the associated recorder.  This value should be sent when a recorder is started, so that the recording list is cleared of any previous recordings by this recorder.  This ensures that traffic control does not remain on permanently due to a recorder crashing and hence not indicating the end of a recording.
# The supplied pairs of paths are treated in descending order of priority, so that the script will attempt to copy all the files in all the first pairs before moving on to the second pairs, and so on.  (Offline files would normally be given higher priority than online files, because they are likely to be needed sooner and are smaller so will be copied more quickly.)  Paths are saved in configuration file PATHS_FILE to allow copying to commence without client intervention when the script is (re)started.  When a client supplies paths, the copying cycle starts or restarts from the highest priority, letting any pending file copy finish first.  This minimises the delay in copying high-priority files without wasting any bandwidth by re-copying.
# In each source directory, every file whose name begins with 8 digits* and ends in ".mxf" (case-insensitive) is examined.  If it does not exist in a subdirectory of the corresponding destination path, named after the first 8 digits of the filename*, it is copied using the COPY executable, into <destination path>/<first 8 digits of filename>*/<DEST_INCOMING>, creating directories if necessary.  After a successful copy, the file is moved up one directory level.  This prevents anything looking at the destination directories from seeing partial files.  At the end of each copying cycle, the (empty) DEST_INCOMING directories are removed.  (* If option -d is provided, 8-digit subdirectories are not used and files do not have to start with 8 digits.)
# If the file already exists in its final resting place, but is of a different length to the source file, a warning is issued and it will not be copied because this might indicate a file name clash.  If a file fails to copy, any partial file is deleted and no more files are copied at that priority (because the order of copying must be maintained - see below).  Once a copy cycle has been completed, the script sleeps until it receives further client connections.  If any files could not be copied, it re-awakes after RECHECK_INTERVAL to try again.
# Because the script creates all required destination directories, it is recommended that mount points are made non-writeable so that non-mounted drives are detected rather than destination directories being created at, and data copied to, the mount point.
# For each priority, the files to copy are sorted by decreasing age using ctime, making sure that all files with the latest ctime have been accounted for.  Before copying a file, the amount of free space is checked to make sure it is likely to fit, and the priority is abandoned if it doesn't: this avoids wasting time repeatedly copying large files that won't succeed.  When copying of a particular priority is completed, the ctime of the most recent successfully-copied file is written to the configuration file, and any files older than this are subsequently ignored.  This allows files to be removed from the destination server without them being copied again if they remain on the recorder.  (ctime is used rather than mtime, because it indicates the time the source file was made visible by moving it into the source directory, therefore making it impossible for older files to appear subsequently, which would never be copied.)
# There must not be more than one destination path for any source path (or the latest destination path will apply).  There is no way to remove path pairs automatically - these must be removed from PATHS_FILE while the script is not running.
# If EXTRA_DEST is not empty, it forms an additional destination for all files at the highest priority.  This is intended for use with a portable drive onto which to copy rushes.
# As source directories are scanned, source files are deleted if they have been successfully copied (indicated by existing at the destination with the same size, or having a ctime older than the value stored in PATHS_FILE), and their mtimes are more than KEEP_PERIOD old.

# The script also runs a very simple HTTP server.  This is configured to return JSON-formatted data at http://<host>:<MON_PORT>/ (status data) or .../availability (a "ping").

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
use constant PERMISSIONS => '0775'; #for directory creation
#use constant COPY => '/home/ingex/bin/cpfs'; #for copying at different speeds
use constant COPY => './cpfs'; #for copying at different speeds
use constant SLOW_LIMIT => 20000; #bandwidth limit as supplied to COPY
use constant FAST_LIMIT => 0; #bandwidth limit as supplied to COPY (0 = unlimited)
use constant RECHECK_INTERVAL => 10; #time (seconds) between rechecks if something failed
use constant EXTRA_DEST => ''; #an extra destination to copy priority 1 files to (from all sources) - e.g. a portable drive.  Ignored if empty.
use constant PATHS_FILE => 'paths';

use vars qw($childPid %opts);

die "Usage: xferserver.pl [-c] [-d] [-p] [-r] [-s]
 -c  clear stored ctime values (re-copy anything not found on server)
 -d  do not make and copy to 8-digit subdirectories
 -p  preserve files (do not delete anything)
 -r  reset (remove stored information)
 -s  start in slow (traffic-limited) mode - NB will switch to fast as soon as ANY recording stops\n" unless getopts 'cdprs', \%opts;

my %transfers;
$transfers{limit} = ($opts{'s'} |= 0); #quotes stop highlighting getting confused in kate!
$transfers{pid} = 0;

my $share = IPC::ShareLite->new( #use a my variable and pass it as a parameter or the signal is not
	-create => 'yes',
	-destroy => 'yes',
	-size => 10240 #default size of 65k is a bit excessive for this app!
) or die $!;
$share->store(freeze \%transfers);

my $t2 = thaw($share->fetch);
#start the copying thread
#trap signals to ensure copying process is killed and shared memory segment is released
my $terminate;
$SIG{INT} = \&Abort;
$SIG{TERM} = \&Abort;
if (!defined ($childPid = open(CHILD, "|-"))) {
	die "failed to fork: $!\n";
}
elsif ($childPid == 0) { # child - copying
	&childLoop($share);

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
			my $transfers = thaw($share->fetch);
			#be strict in reading the file because mistakes could lead to data loss (wrong ctimeCleared causing files which haven't been copied to be deleted)
			while (@config) {
				my $priority = shift @config;
				unless ($priority =~ /^ (\d+)$/) {
					print "Unrecognised priority value: \"$priority\": ignoring rest of file.\n";
					last;
				}
				$priority = $1;
				unless (@config) {
					print "Missing ctimeCleared value: ignoring rest of file.\n";
					last;
				}
				my $ctimeCleared = shift @config;
				unless ($ctimeCleared !~ /\D/) {
					print "Unrecognised ctimeCleared value: \"$ctimeCleared\": ignoring rest of file.\n";
					last;
				}
				unless (@config) {
					print "No pairs for priority $priority: ignoring rest of file.\n";
					last;
				}
				do {
					addPair($transfers, \@config, $priority, $opts{c} ? 0 : $ctimeCleared);
				} while (@config && $config[0] !~ /^ /);
			}
			$share->store(freeze $transfers);
			$share->unlock;
			if ($transfers->{transfers}) {
				print "\n";
				print CHILD "\n"; #kicks the child into action if it's idle
			}
			else {
				print "WARNING: No valid transfers found\n\n";
			}	
		}
		else {
			print "WARNING: Couldn't read from '", PATHS_FILE, "': $!\n";
		}
	}
	else {
		print "Empty '", PATHS_FILE, "'.\n\n";
	}
}
else {
	print "No stored paths file '", PATHS_FILE, "' found.\n\n";
}

my $ports = new IO::Select;

#open the socket for instructions from recorders via xferclient.pl
my $recSock = new IO::Socket::INET(
 	LocalPort => CLIENT_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . CLIENT_PORT . ": $!\n");
$ports->add($recSock);
Report("Listening for recorder clients on port " . CLIENT_PORT . ".\n");

#open the socket for status requests from web browser-type things
my $monSock = new IO::Socket::INET(
 	LocalPort => MON_PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or Abort('Could not create server socket at port ' . MON_PORT . ": $!\n");
$ports->add($monSock);
Report("Listening for status requests at http://localhost:" . MON_PORT . "/.\n\n");

my %recordings;

#main loop
while (1) {
	my @requests = $ports->can_read;
	foreach (@requests) {
 		my $client = $_->accept();
		if ($_ == $recSock) {
			&serveRecorder($client, $share);
		}
		else {
			&serveStatus($client);
		}
		$client->shutdown(2); #stopped using it - this seems to prevent partial status pages being served (when close() is used instead)
	}
}

sub serveRecorder {
 my ($client, $share) = @_;
 my ($buf, $data);
 while (sysread($client, $buf, 100)) { #don't use buffered read or can hang if several strings arrive at once
	$data .= $buf;
 }
 if (!defined $data) {
	print "\rWARNING: Recorder client connection made but no parameters passed: ignoring.\n";
	return;
 }
 my @commands = split /\n/, $data;
 my $recorder = shift @commands;
 my $index = shift @commands;
 if (!defined $index) {
 	print "\rWARNING: Recorder '$recorder' provided no index: ignoring.\n";
	return;
 }
 my $priority = 1;
 if (@commands) { #not recording
	print "\rRecorder '$recorder' says copy:\n";
	$priority = 1;
	chomp @commands;
	if ($index eq '0') { #all recordings
		delete $recordings{$recorder};
	}
	else {
		delete $recordings{$recorder}{$index};
		if (!keys %{ $recordings{$recorder} }) {
			delete $recordings{$recorder};
		}
	}
	$share->lock(LOCK_EX);
	my $transfers = thaw($share->fetch);
	while (@commands) {
		addPair($transfers, \@commands, $priority++);
	}
	print CHILD "\n"; #kicks the child into action if it's idle
	if (!keys %recordings && $transfers->{limit}) {
		$transfers->{limit} = 0;
		kill 'USR2', $transfers->{pid} if $transfers->{pid};
		print "\rTRAFFIC CONTROL OFF\n";
	}
	$share->store(freeze $transfers);
	SaveConfig($transfers); #so that if the script is re-started it can carry on copying immediately
	$share->unlock;
 }
 else { #recording
	print "\rRecorder '$recorder' recording (index '$index').\n";
	print "\r WARNING: index is 0 so all recordings from this recorder will have assumed to have stopped when this recording stops, possibly leading to an erroneous removal of traffic control.\n" unless $index;
	#traffic control
	$recordings{$recorder}{$index} = 1;
	$share->lock(LOCK_EX);
	my $transfers = thaw($share->fetch);
	if (!$transfers->{limit}) {
		$transfers->{limit} = 1;
		$share->store(freeze $transfers);
		kill 'USR1', $transfers->{pid} if $transfers->{pid};
		print "\rTRAFFIC CONTROL ON\n";
	}
	$share->unlock;
 }
}

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
	if (keys %recordings) {
		my @output;
		foreach (sort keys %recordings) { #there will always be at least one pair for each priority
			push @output, "\t\t\t{ \"name\" : " . E($_) . ', "number" : ' . keys(%{ $recordings{$_} }) . ' }';
		}
		print $client join(",\n", @output), "\n";
	}
	print $client "\t\t],\n";

	print $client "\t\t\"endpoints\" : {\n";
	my $transfers = thaw($share->fetch); #fetch is atomic so no need to lock as not changing $transfers
	if (keys %{ $transfers->{transfers} }) {
		my @priorities;
		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			my %pairs = %{ $transfers->{transfers}{$priority}{pairs} };
			my @output;
			foreach (sort keys %pairs) { #there will always be at least one pair for each priority
				push @output, "\t\t\t\t{ \"source\" : " . E($_) . ', "destination" : ' . E($pairs{$_}) . ' }';
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

sub E {
	my $string = shift;
	$string =~ s/\\/\\\\/g;
	$string =~ s/"/\\"/g;
	return "\"$string\"";
}

sub SaveConfig { #should be locked before doing this to prevent concurrent file writes
	my $transfers = shift;
 	my $config = IO::File->new('>' . PATHS_FILE);
 	if ($config) {
 		foreach my $priority (sort keys %{ $transfers->{transfers} }) {
			print $config " $priority\n$transfers->{transfers}{$priority}{ctimeCleared}\n";
			my %pairs = %{ $transfers->{transfers}{$priority}{pairs} };
 			foreach (sort keys %pairs) {
 				print $config "$_\n$pairs{$_}\n";
 			}
 		}
 		$config->close;
 	}
 	else {
 		Report("WARNING: couldn't open \"" . PATHS_FILE . "\" for writing: $!.\nTransfers will not recommence automatically if script is restarted.\n");
 	}
}

sub addPair {
 my ($transfers, $dirs, $priority, $ctimeCleared) = @_;
 my $source = shift @$dirs;
 $source =~ s|/*$||o;
 unless (@$dirs && $$dirs[0] =~ /^\S/) {
	print "\rWARNING: source directory '$source/' has no corresponding destination directory: ignoring\n";
	return;
 }
 my $dest = shift @$dirs;
 $dest =~ s|/*$||o;
 print "\rPriority " . $priority . ": '$source/*' -> '$dest/*'\n";
 if (!-d $source) {
	print "\r WARNING: source directory '$source/' doesn't exist: ignoring\n";
	return;
 }
 if ($priority == 1 && EXTRA_DEST ne '') {
	if (!-d EXTRA_DEST) {
		if (MakeDir(EXTRA_DEST)) {
			print "\r WARNING: cannot create extra destination '" . EXTRA_DEST . "/': $!: ignoring\n";
		}
		else {
			print "\r Created extra destination '" . EXTRA_DEST . "/'\n";
		}
	}
 }
 if (!-d $dest) {
	if (MakeDir($dest)) {
		print "\r WARNING: cannot create '$dest/': $!: ignoring\n";
		return;
	}
	else {
		print "\r Created '$dest/'\n";
	}
 }
 $transfers->{transfers}{$priority}{pairs}{$source} = $dest; #prevents duplication
 if (defined $ctimeCleared) { #value supplied from file
 	$transfers->{transfers}{$priority}{ctimeCleared} = $ctimeCleared;
 }
 elsif (!defined $transfers->{transfers}{$priority}{ctimeCleared}) { #value not previously supplied from file or from copying operations
 	$transfers->{transfers}{$priority}{ctimeCleared} = 0; #copy everything from the epoch onwards
 }
 $transfers->{new} = 1; #gets the child to restart scanning from the highest priority at the end of the current copy.  Do this while still locked after adding the directory pair to prevent the copying thread deleting the directory pair
}

sub Scan {
 my ($handle, $srcDir, $destRoot, $ctimeCleared, $essenceFiles, $retry, $msgs) = @_;
 my ($totalFiles, $totalSize, $latestCtime) = (0, 0, 0);
 while (my $fname = readdir $handle) {
	my $destDir;
	if ($opts{d}) { #don't use subdirectories
		#check the filename
		next unless $fname =~ /\.[mM][xX][fF]$/o;
		$destDir = $destRoot;
	}
	else {
		#check the filename
		next unless $fname =~ /^(\d{8})_.*\.[mM][xX][fF]$/o;
		$destDir = "$destRoot/$1";
	}
	my $fullSource = "$srcDir/$fname";
	#get info about the file
	#Use ctime to test for files to be ignored that have been copied already.
	#This is because ctime is updated when the files appear in the source directories,
	#so we can guarantee that files with an earlier ctime won't appear after we've copied everything up to a certain ctime,
	#which could otherwise result in files not being copied.
	#Use mtime to test for deleting old files, because if ctime has changed for some spurious reason (permissions/ownership were modified, perhaps?),
	#it's still safe to delete the files.
	my ($dev, $size, $mtime, $ctime) = (lstat $fullSource)[0, 7, 9, 10]; 
	unless ($dev) { #if lstat fails then it returns a null list
		Report("WARNING: failed to lstat '$fullSource': $!: not copying\n");
		$$retry = 1;
		$$msgs = 1;
		next;
	}
	unless ($size) {
		Report("WARNING: '$fullSource' has zero length: ignoring\n");
		$$msgs = 1;
		next;
	}
	#compare with destination file, if it exists
	my $fullDest = "$destDir/$fname";
	if (-e $fullDest) {
		#warn and skip if the file size differs (possibly due to filename clash in system)
		if ($size != -s $fullDest) {
			Report("WARNING: '$fullDest' exists and is a different size (" . (-s $fullDest) . ") from '$fullSource' ($size): may be a filename conflict: not copying\n");
			$$retry = 1;
			$$msgs = 1;
		}
		#remove source file if it's old
		elsif (time > $mtime + KEEP_PERIOD) {
			if ($opts{p}) {
				Report("Would remove old file '$fullSource' if not prevented: copy found on server\n");
			}
			else {
				Report("Removing old file '$fullSource': copy found on server\n");
				unlink $fullSource or Report("WARNING: could not remove '$fullSource': $!\n");
			}
			$$msgs = 1;
		}
		next;
	}
	#don't copy if already copied; remove if old and already copied
	elsif ($ctime <= $ctimeCleared) { #file has already been copied (but has disappeared off the server)
		if (time > $mtime + KEEP_PERIOD) {
			if ($opts{p}) {
				Report("Would remove old file '$fullSource' if not prevented: copy not on server but previously copied\n");
			}
			else {
				Report("Removing old file '$fullSource': copy not on server but previously copied\n");
				unlink $fullSource or Report("WARNING: could not remove '$fullSource': $!\n");
			}
			$$msgs = 1;
		}
		next;
	}
	#add to the list for copying
	push @$essenceFiles, {
		fname => $fname,
		fullsource => $fullSource,
		destRoot => $destRoot,
		destDir => $destDir,
		mtime => $mtime,
		ctime => $ctime,
		size => $size,
	};
	#update stats
	$totalFiles++;
	$totalSize += $size;
	$latestCtime = $ctime if $ctime > $latestCtime;
 }
 return ($totalFiles, $totalSize, $latestCtime);
}

sub childLoop {
 my $share = shift;
 $SIG{INT} = sub { exit; };
 $SIG{TERM} = sub { exit; };
 $SIG{USR1} = 'IGNORE';
 $SIG{USR2} = 'IGNORE';
 local $| = 1; #autoflush stdout

 my $retry = 0;
 my $msgs = 1; #print the idle message the first time round
 while (1) {

	#wait for main thread or retry
	my $rin = "";
	vec($rin, fileno(STDIN), 1) = 1;
	if (!select $rin, undef, $rin, $retry ? RECHECK_INTERVAL : undef) {
		Report("Retrying...\n");
	}
	else { #kicked by the main thread
		<STDIN>;
	}
	$retry = 0;
	$share->lock(LOCK_EX);
	my $transfers = thaw($share->fetch);
	$transfers->{new} = 0; #checking from the top level so don't need to be told to do so
	$share->store(freeze $transfers);
	$share->unlock;
	my %incomingDirs;
	my @priorities = sort keys %{ $transfers->{transfers} };
	while (@priorities) {
		my $priority = shift @priorities;
		my $priRetry = 0;
		#get src/dest pairs for this priority
		$transfers = thaw($share->fetch); #fetch is atomic so no need to lock as not changing $transfers
		my %pairs = %{ $transfers->{transfers}{$priority}{pairs} };
		my $ctimeCleared = $transfers->{transfers}{$priority}{ctimeCleared};
		#make a list of all the files to copy at this priority
		my @essenceFiles;
		my ($totalFiles, $totalSize);
		foreach (sort keys %pairs) {
			if (!opendir(SRC, $_)) {
				Report("WARNING: failed to open source directory '$_/': $!: ignoring\n");
				$$priRetry = 1;
				$msgs = 1;
			}
			else {
				my (@stats, @files);
				my $scanTime = time;
				do { #loop to make sure we get all the files with the latest ctime we find
					@files = ();
					$scanTime = time;
					@stats = Scan(*SRC, $_, $pairs{$_}, $ctimeCleared, \@files, \$priRetry, \$msgs);
					rewinddir SRC; #looks at the directory again!
				} while ($stats[2] >= $scanTime); #in the unlikely event we find files with the same or later ctime as when (just before) the scan started, scan again in case more files have appeared with the same ctime
				push @essenceFiles, @files;
				$totalFiles += $stats[0];
				$totalSize += $stats[1];
				if ($priority == 1 && EXTRA_DEST ne '') {
					@stats = Scan(*SRC, $_, EXTRA_DEST, $ctimeCleared, \@essenceFiles, \$priRetry, \$msgs);
					$totalFiles += $stats[0];
					$totalSize += $stats[1];
				}
				closedir(SRC);
			}
		}
		$share->lock(LOCK_EX);
		$transfers = thaw($share->fetch);
		$transfers->{current}{priority} = $priority;
		$transfers->{current}{totalFiles} = $totalFiles;
		$transfers->{current}{totalSize} = $totalSize;
		$share->store(freeze $transfers);
		$share->unlock;
		my $prevCtime = $transfers->{transfers}{$priority}{ctimeCleared};
		#copy the files, oldest first
		foreach my $essenceFile (sort { $a->{ctime} <=> $b->{ctime} } @essenceFiles) {
			$share->lock(LOCK_EX);
			$transfers = thaw($share->fetch);
			if (!$priRetry && $essenceFile->{ctime} > $prevCtime) { #have copied all files older than current file's ctime
				$transfers->{transfers}{$priority}{ctimeCleared} = $prevCtime;
				SaveConfig($transfers);
			}
			$transfers->{current}{fileName} = $essenceFile->{fullsource};
			$transfers->{current}{fileSize} = $essenceFile->{size};
			$transfers->{current}{fileProgress} = 0; #in case a status request is made now
			$share->store(freeze $transfers);
			$share->unlock;
			# check/create root destination dir
			if (!-d $essenceFile->{destRoot} && MakeDir($essenceFile->{destRoot})) {
		 		Report("WARNING: Couldn't create '" . $essenceFile->{destRoot} . "': $!\n");
				$msgs = 1;
				$priRetry = 1;
				last; # can't do this on a per-destination basis as it would break the ctime order
			}
			# check free space
			my $free = dfportable($essenceFile->{destRoot})->{bfree};
			if ($free < $essenceFile->{size}) { # a crude test but which will catch most instances of insufficient space.  Assumes subdirs are all on the same partition
				Report("Prio $priority: '" . $essenceFile->{destRoot} . "' full ($free byte" . ($free == 1 ? '' : 's') . " free; file size " . $essenceFile->{size} . " bytes): not attempting to copy anything else at this priority.\n");
				$msgs = 1;
				$priRetry = 1;
				last; # can't do this on a per-destination basis as it would break the ctime order
			}
			# check/create copy subdir
			my $incomingPath = $essenceFile->{destDir} . '/' . DEST_INCOMING;
			if (!-d $incomingPath && MakeDir($incomingPath)) {
			 	Report("WARNING: Couldn't create '$incomingPath': $!\n");
				$msgs = 1;
				$priRetry = 1;
				last; # can't do this on a per-destination basis as it would break the ctime order
			}
			$incomingDirs{$incomingPath} = 1;
			# copy the file
			$prevCtime = $essenceFile->{ctime};
			Report("Prio $priority (" . $transfers->{current}{totalFiles} . " to go):'" . $essenceFile->{fullsource} . "' -> '" . $incomingPath . "/':\n");
			$msgs = 1;
			my $childMsgs;
			{ # block to limit scope of signal handlers
				my $cpPid;
				local $SIG{INT} = sub { kill 'INT', $cpPid; };
				local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
				# (doing the above with the USR signals doesn't seem to work properly when using a forking open rather than a fork)
				# run copy program with a forking open so that we can get its stdout
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
						$essenceFile->{fullsource}, #source file
						"$incomingPath/" . $essenceFile->{fname} #dest file
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
				# get progress from the copy whilw waiting for it to finish
				my $buf;
				my $signalled = 0;
				while (sysread(COPYCHILD, $buf, 100)) { # can't use <> because there aren't any newlines
					if (!$signalled) { # first time the copy prog has printed anything so it's just come alive
						$signalled = 1; # only need to do this once
						kill 'USR2', $cpPid unless $transfers->{limit}; #switch to fast copying
					}
					$childMsgs .= $buf;
					if ($childMsgs =~ /(\d+)%/) {
						$share->lock(LOCK_EX);
						$transfers = thaw($share->fetch);
						$transfers->{current}{fileProgress} = $1;
						$share->store(freeze $transfers);
						$share->unlock;
						print $childMsgs;
						$childMsgs = '';
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
			# check result
 			if ($? & 127) { #child has received terminate signal so terminate
				unlink "$incomingPath/" . $essenceFile->{fname};
				exit 1; # TODO: use more exit code values
			}
			if ($? >> 8) { #copy failed
				Report("WARNING: copy failed, exiting with value " . ($? >> 8) . ": $childMsgs\n");
				unlink "$incomingPath/" . $essenceFile->{fname};
				$msgs = 1;
				$priRetry = 1;
				last; # have to abandon the whole priority or it would break the ctime order
			}
			# move the file from the incoming directory to the live directory
			print ". Moving...";
			if (!rename "$incomingPath/" . $essenceFile->{fname}, $essenceFile->{destDir} . '/' . $essenceFile->{fname}) {
				Report("\nWARNING: Failed to move file to '" . $essenceFile->{destDir} . "/': $!");
				$msgs = 1;
				$priRetry = 1;
				last; # have to abandon the whole priority or it would break the ctime order
			}
			print " Done\n";
			# update stats
			$share->lock(LOCK_EX);
			$transfers = thaw($share->fetch);
			$transfers->{current}{totalFiles}--;
			$transfers->{current}{totalSize} -= $essenceFile->{size};
			$share->store(freeze $transfers);
			#check if new files may have appeared during this copy
			$share->unlock;
			if ($transfers->{new} && $priority > 1) {
				#stop copying files at this priority in case higher priority files have appeared (if at priority 1, files that have appeared will be newer so would be copied later anyway)
				last;
			}
		} #file copy loop
		$share->lock(LOCK_EX);
		$transfers = thaw($share->fetch);
		delete $transfers->{current};
		if (!$priRetry) {
			#no more files so must have copied everything at the last file's ctime
			$transfers->{transfers}{$priority}{ctimeCleared} = $prevCtime;
			SaveConfig($transfers);
			$share->store(freeze $transfers);
		}
		$share->unlock;
		if ($transfers->{new}) { #new files have arrived
 			if ($priority > 1) {
				@priorities = (); #exit priorities loop
				Report("Prio $priority: Pausing copying to check for new files of a higher priority\n");
			}
		}
		elsif ($priRetry) {
			Report("Prio $priority: All copyable files copied.\n") if $msgs;
		}
		else {
			Report("Prio $priority: All files successfully copied.\n");
		}
		$retry |= $priRetry;
	} # priorities loop
	# remove temp incoming directoris
	foreach (keys %incomingDirs) {
		rmdir;
	}
	# print status
	if ($msgs) {
		if ($retry) {
			Report('Pausing for ' . RECHECK_INTERVAL . " seconds before retrying.\n\n");
		}
		else {
			print "Idle.\n\n";
		}
	}
	$msgs = 0;
 }
}

sub Abort {
	$terminate = 1;
	kill "INT", $childPid;
	waitpid($childPid, 0); #so that it's safe to remove the shared object
	my $msg = shift;
	$msg |= '';
	die "\nAborting: $msg\n";
}

sub MakeDir { #makes a directory (recursively) without the permissions being modified by the current umask
	return system 'mkdir', '-p', '-m', PERMISSIONS, shift;
}

sub Report {
	printf '%02d:%02d:%02d %s', (localtime(time))[2, 1, 0], shift;
}