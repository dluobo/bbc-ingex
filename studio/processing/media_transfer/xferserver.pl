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

# Listens on network port PORT for clients to open connections, send parameters and then close connections.
# Parameters are as follows:
# <recorder name> <recording index> [<source path> <destination path>] ... (all terminated with newlines)
# The recorder name and index combination identify a recording, and must be unique so that the script can determine unambiguously if any recordings are taking place, even when a recorder starts a recording before finishing the previous one due to postroll.
# If No other parameters are supplied, this indicates the start of a recording.  The recording's identifier is added to a recording list, and traffic control is switched on at rate LIMIT as supplied to the copy command.
# Additional pair(s) of paths indicate the end of a recording, or a recorder being started.  They will result in the recording's identifier being removed from the recording list (if present), and if the list is empty, traffic control is switched off.
# Index values can be arbitrary except for the special case zero, which is interpreted as covering all recordings being made by the current recorder.  This value should be sent when a recorder is started, so that the recording list is cleared of any previous recordings by this recorder.  This ensures that traffic control does not remain on permanently due to a recorder crashing and hence not indicating the end of a recording.
# Copying occurs in a child process, to keep the socket and traffic control responsive.  For each source path, COPY is used to copy every file that begins with 8 digits and ends in ".mxf" (case-insensitive).  The file will be copied into subdirectory DEST_INCOMING of a directory named as the first 8 digits of the filename, which is itself a subdirectory of the corresponding destination path.  After a successful copy, the file is moved up one directory level.  This prevents anything looking at the destination directories from seeing partial files.
# The script will create all required destination directories. It is therefore recommended that mount points are made non-writeable so that non-mounted drives are detected rather than destination directories being created at, and data copied to, the mount point.
# Files that cannot be copied are skipped with a warning.
# The first pair of paths sent by every client are grouped together as the highest priority, and all files in these source directories will be copied first, in modification time order of oldest to newest.  When an attempt has been made to copy all these files, the group of second pairs of paths will be processed, and so on.  When all paths have been scanned, the script will idle until a client sends some path pairs, indicating a newly-started recorder or recording completion, and then the scanning process will begin again.  If a client sends path pairs during copying, at the end of the current file copy the scanning process will begin again, ensuring that high priority copies are not delayed while a large number of low priority copies are made.
# There must not be more than one destination path for any source path (or the latest destination path will apply).  There is no way to remove path pairs.
# If EXTRA_DEST is not empty, it forms an additional destination for all files at the highest priority.  This is intended for use with a portable drive onto which to copy rushes.
# As directories are scanned, successfully-copied source files (as determined by file size) are deleted if their modification times are more than KEEP_PERIOD old.  Source files that are apparently copied (i.e. reside in the final destination directory) but whose size does not match are ignored with a warning, as they may indicate a file name clash.
# Any problems will result in another copying attempt after RECHECK_INTERVAL seconds.

use strict;
use IO::Socket;
use IPC::Shareable; #this will need to be installed from CPAN
#use XML::Simple; #this will need to be installed from CPAN

use constant PORT => 2000;
use constant DEST_INCOMING => 'incoming'; #incoming file subdirectory
use constant KEEP_PERIOD => 24 * 60 * 60 * 7; #a week (in seconds)
use constant PERMISSIONS => '0775'; #for directory creation
use constant COPY => '/home/ingex/bin/cpfs'; #for copying at different speeds
use constant LIMIT => 20000; #bandwidth limit as supplied to COPY
use constant RECHECK_INTERVAL => 10; #time (seconds) between rechecks if something failed
use constant EXTRA_DEST => ''; #an extra destination to copy priority 1 files to (from all sources) - e.g. a portable drive.  Ignored if empty.

use vars qw($handle %transfers);

# system 'sudo /usr/sbin/tc qdisc del dev eth0 root >& /dev/null'; #tc will fail unless stuff lying around
$handle = tie %transfers, 'IPC::Shareable', undef, {destroy=>1}; #destroys shared memory segment on program exit
$transfers{limit} = 0;

my $sock = new IO::Socket::INET(
 	LocalPort => PORT,
# 	Type => SOCK_STREAM,
 	Reuse => 1,
 	Listen => 100,
 	) or die 'Could not create server socket at port ' . PORT . ": $!\n";

report("Listening on port " . PORT . ".\n");

my $childPid;
#trap signals to ensure copying process is killed and shared memory segment is released
my $terminate;
$SIG{INT} = \&Abort;
$SIG{TERM} = \&Abort;
if (!defined ($childPid = open(CHILD, "|-"))) {
	die "failed to fork: $!\n";
}
elsif ($childPid == 0) { # child - copying
	$SIG{INT} = sub { exit; };
	$SIG{TERM} = sub { exit; };
	$SIG{USR1} = 'IGNORE';
	$SIG{USR2} = 'IGNORE';
	local $| = 1; #autoflush stdout

	my $ok = 1; #no problems yet
	my $msgs = 1; #print the idle message the first time round
 	while (1) {
		#wait for main thread or retry
 		my $rin = "";
 		vec($rin, fileno(STDIN), 1) = 1;
 		if (!select $rin, undef, $rin, $ok ? undef : RECHECK_INTERVAL) {
 			report("Retrying...\n");
 		}
 		else { #kicked by the main thread
 			<STDIN>;
 		}
		$ok = 1;
		$handle->shlock;
		$transfers{retry} = 0; #checking from the top level so don't need to be told to do so
		$handle->shunlock;
		my $continue = 1;
		my $priority = 0;
		while ($continue) { #priorities loop
			#get src/dest pairs for this priority
			$handle->shlock;
			unless ($transfers{++$priority}) {
				#finished
				$handle->shunlock;
				last;
			}
			my %pairs = %{ $transfers{$priority} };
			$handle->shunlock;
			#make a list of all the files at this priority
			my @essenceFiles;
			foreach (sort keys %pairs) {
				if (!opendir(SRC, $_)) {
 					report("WARNING: failed to open source directory '$_/': $!: ignoring\n");
					$ok = 0;
					$msgs = 1;
					next;
				}
				scan(*SRC, $pairs{$_}, \@essenceFiles, \$ok, \$msgs);
				if ($priority == 1 && EXTRA_DEST ne '') {
					rewinddir(SRC);
					scan(*SRC, EXTRA_DEST, \@essenceFiles, \$ok, \$msgs);
				}
				closedir(SRC);
			}
			#copy the files, oldest first
			foreach my $essenceFile (sort { $a->{"mtime"} <=> $b->{"mtime"} } @essenceFiles) {
				#check the incoming directory
				my $incomingPath = $essenceFile->{destpath} . '/' . DEST_INCOMING;
				#copy the file
				report("Prio $priority: '" . $essenceFile->{fullsource} . "' -> '" . $incomingPath . "/':\n");
				$msgs = 1;
				{ #block to limit scope of signal handlers
					my $cpPid;
					local $SIG{INT} = sub { kill 'INT', $cpPid; };
					local $SIG{TERM} = sub { kill 'TERM', $cpPid; };
					local $SIG{USR1} = sub { kill 'USR1', $cpPid; }; #limit
					local $SIG{USR2} = sub { kill 'USR2', $cpPid; }; #remove limit
					#run copy program in a child process so it can be interrupted
					if (!defined ($cpPid = fork())) {
						die "failed to fork: $!";
					}
					elsif ($cpPid == 0) { # child
						$handle->shlock;
						my $limit = $transfers{limit};
						$handle->shunlock;
						exec COPY,
							$limit, #whether to start bandwidth-limited
							LIMIT, #the bandwidth limit
							$essenceFile->{fullsource}, #source file
							"$incomingPath/" . $essenceFile->{fname}; #dest file
						die "failed to exec copy command: $!";
					}
					#wait for rsync to finish or be interrupted
					waitpid($cpPid, 0);
				}
				# check exit of wait call and rsync exec
				if ($? == -1) { #copy failed to execute
					print "WARNING: Couldn't execute copy command: $!: not copying\n";
					$ok = 0;
					next;
				}
				if ($? & 127) { #child has received terminate signal so terminate
					exit(1); # TODO: use more exit code values
				}
				if ($? >> 8) { #copy failed
					print "WARNING: copy failed, exiting with value " . ($? >> 8) . ": not copying\n";
					$ok = 0;
					next;
				}
				# move the file from the incoming directory to the live directory
				print ". Moving...";
				if (!rename "$incomingPath/" . $essenceFile->{fname}, $essenceFile->{destpath} . '/' . $essenceFile->{fname}) {
					print "\nWARNING: Failed to move file to '" . $essenceFile->{destpath} . "/': $!";
					$ok = 0;
				}
				print "\n";
				#check if new files may have appeared during this copy
				$handle->shlock;
				if ($transfers{retry} && $priority > 1) {
					#stop copying files at this priority in case higher priority files have appeared
					$handle->shunlock;
					$continue = 0; #exit outer loop
					last;
				}
				$handle->shunlock;
			}
			if ($continue) {
				report("Prio $priority: All copyable files copied.\n") if $msgs;
			}
			else {
				report("Prio $priority: Pausing copying to check for new files of a higher priority\n");
			}
		}
		if ($msgs) {
			if ($ok) {
				print "Idle.\n\n";
			}
			else {
				report('Pausing for ' . RECHECK_INTERVAL . " seconds before retrying.\n\n");
			}
		}
		$msgs = 0;
	}
}
CHILD->autoflush(1);

my %recordings;
my $recording = 0;

my ($opened, $closed);

while (my $conn = $sock->accept()) {

# my $rin = "";
# vec($rin, fileno($sock), 1) = 1;
# while (1) {
# 	select $rin, undef, $rin, 10;
# 	my $conn = $sock->accept();

#	print "New connection...\n";
	my $recorder = <$conn>;
	if (!defined $recorder) {
		print "\rWARNING: connection made but no parameters passed: ignoring\n";
		next;
	}
	chomp $recorder;
	my $index = <$conn>;
	if (!defined $index) {
		print "\rWARNING: recorder '$recorder' provided no index: ignoring\n";
		next;
	}
	chomp $index;
	my @dirs = <$conn>;
	if (@dirs) { #not recording
		print "\rRecorder '$recorder' says copy:\n";
		my $priority = 0;
		chomp @dirs;
		while (@dirs) {
			$priority++;
			my $source = shift @dirs;
			$source =~ s|/*$||o;
			unless (@dirs) {
				print "\rWARNING: source directory '$source/' has no corresponding destination directory: ignoring\n";
				last;
			}
			my $dest = shift @dirs;
			$dest =~ s|/*$||o;
			print "\r" . $priority . ". '$source/*' -> '$dest/*'\n";
			if (!-d $source) {
				print "\r WARNING: source directory '$source/' doesn't exist: ignoring\n";
				next;
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
					next;
				}
				else {
					print "\r Created '$dest/'\n";
				}
			}
			$handle->shlock;
			$transfers{$priority}{$source} = $dest;
			$handle->shunlock;
		}
		#inform the child
		$handle->shlock;
		$transfers{retry} = 1; #gets the child to restart scanning at the end of the current copy if it's copying
		$handle->shunlock;
		print CHILD "\n"; #kicks the child into action if it's idle
		#traffic control
		if ($index eq '0') { #all recordings
			delete $recordings{$recorder};
		}
		else {
			delete $recordings{$recorder}{$index};
			if (!keys %{ $recordings{$recorder} }) {
				delete $recordings{$recorder}
			}
		}
		if ($recording && !keys %recordings) {
			$recording = 0;
			$handle->shlock;
			$transfers{limit} = 0;
			$handle->shunlock;
			kill 'USR2', $childPid;
# 			unless (system 'sudo', '/usr/sbin/tc', 'qdisc', 'del', 'dev', 'eth0', 'root') {
 				print "\rTRAFFIC CONTROL OFF\n";
# 			}
#  			else {
# 				print "WARNING: Couldn't delete qdisc to turn traffic control off: $!\n";
# 			}
		}
	}
	else { #recording
		print "Recorder '$recorder' recording (index '$index').\n";
		$recordings{$recorder}{$index} = 1;
		if (!$recording) {
			$recording = 1;
			$handle->shlock;
			$transfers{limit} = 1;
			$handle->shunlock;
			kill 'USR1', $childPid;
#			Abort("Couldn't create qdisc: $!") if system 'sudo', '/usr/sbin/tc', 'qdisc', 'add', 'dev', 'eth0', 'root', 'handle', '1:', 'cbq', 'avpkt', '1000', 'bandwidth', '1000mbit';
#			Abort("Couldn't create class: $!") if system 'sudo', '/usr/sbin/tc', 'class', 'add', 'dev', 'eth0', 'parent', '1:', 'classid', '1:1', 'cbq', 'rate', '400mbit', 'allot', '1500', 'prio', '5', 'bounded', 'isolated';
#			Abort("Couldn't add filter: $!") if system 'sudo', '/usr/sbin/tc', 'filter', 'add', 'dev', 'eth0', 'parent', '1:', 'protocol', 'ip', 'prio', '16', 'u32', 'match', 'ip', 'dport', '445', '0xffff', 'flowid', '1:1';
			print "\rTRAFFIC CONTROL ON\n";
		}
	}
}

sub scan {
	my ($handle, $dest, $essenceFiles, $ok, $msgs) = @_;
	while (my $fname = readdir $handle) {
		#check the filename
		next unless $fname =~ /^(\d{8})_.*\.[mM][xX][fF]$/o;
		#check the dest directory
		my $destPath = "$dest/$1";
		my $fullSource = "$_/$fname";
		if (!-d $destPath && MakeDir($destPath)) {
			report("WARNING: Couldn't create '$destPath/': $!: not copying '$fullSource'\n");
			$$ok = 0;
			$$msgs = 1;
			next;
		}
		#check the incoming directory
		my $incomingPath = $destPath . '/' . DEST_INCOMING;
		if (!-d $incomingPath && MakeDir($incomingPath)) {
			report("WARNING: Couldn't create '$incomingPath/': $!: not copying '$fullSource'\n");
			$$ok = 0;
			$$msgs = 1;
			next;
		}
		#get info about the file
		my ($dev, $size, $mtime) = (lstat $fullSource)[0, 7, 9];
		unless ($dev) { #if lstat failed then is returns a null list
			report("WARNING: failed to lstat '$fullSource': $!: not copying\n");
			$$ok = 0;
			$$msgs = 1;
			next;
		}
		#compare with destination file, if it exists
		my $fullDest = "$destPath/$fname";
		if (-e $fullDest) {
			#warn and skip if the file size differs (possibly due to filename clash in system)
			if (-s $fullSource != -s $fullDest) {
				report("WARNING: '$fullDest' exists and is a different size (" . -s $fullDest . ") than '$fullSource' (" . -s $fullSource . "(: may be a filename conflict: not copying\n");
				$$ok = 0;
				$$msgs = 1;
				next;
			}
			#remove source file if it's old
			if (time > $mtime + KEEP_PERIOD) {
				report("Removing old file '$fullSource'\n");
				unlink $fullSource or report("WARNING: could not remove '$fullSource': $!\n");
				$$msgs = 1;
			}
			next;
		}
		#add to the list for copying
		push @$essenceFiles, {
			fname => $fname,
			fullsource => $fullSource,
			destpath => $destPath,
			mtime => $mtime,
		};
	}
}

sub Abort {
	#switch off traffic control
	system 'sudo /usr/sbin/tc qdisc del dev eth0 root >& /dev/null'; #tc will fail unless stuff lying around
	$terminate = 1;
	kill "INT", $childPid;
#	waitpid $childPid, 0;
	my $msg = shift;
	$msg |= '';
	die "\nAborting: $msg\n";
}

sub MakeDir { #makes a directory (recursively) without the permissions being modified by the current umask
	return system 'mkdir', '-p', '-m', PERMISSIONS, shift;
}

sub report {
	printf '%02d:%02d:%02d %s', (localtime(time))[2, 1, 0], shift;
}