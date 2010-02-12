#! /usr/bin/perl -w

#  $Id: import_db_infod.pl,v 1.3 2010/02/12 14:51:05 john_f Exp $
#
# Copyright (C) 2009-2010  British Broadcasting Corporation.
# All Rights Reserved.
# Author: Matthew Marks
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#


# Scans $ROOT/@topDirs/<all subdirs>/<all 8-digit subdirs>/ for files ending in .mxf or .xml (case-insensitive), invoking $mxfImport or $xmlImport on them respectively.
# Watches the tree for new directories and files.
# Maintains timestamp files $timestampFile in each 8-digit subdir, holding the latest mtime, so as to avoid re-importing files.

# If run as root, applies UID and GID of $ROOT to each $timestampFile created.

# Dies if $ROOT does not initially exist

# Logs to the system log (/var/log/messages)

use strict;
use Getopt::Std;
use Proc::Daemon;
use Linux::Inotify2;
use File::Path;
use File::Glob qw( :glob); # to ensure whitespace in names returned by glob is escaped
use Sys::Syslog qw(:standard :macros);

my $ROOT = '/store'; #the user/group of this is applied to all files created if the script is run as root
my %topDirs = qw(mxf_offline 1 mxf_online 1 cuts 1); #incoming directories to scan, below $ROOT
my $timestampFile = 'import_db_info_timestamp';
my $mxfImport = '/usr/local/bin/import_mxf_info';
my $xmlImport = '/usr/local/bin/import_cuts';

use constant WATCH_MASK => IN_CREATE | IN_MOVED_TO;

openlog 'import_db_infod', 'perror', 'user'; #'perror' echoes output to stderr (if using -n option)
# check arguments
my %opts;
&usage_exit unless getopts('cnv', \%opts);

sub usage_exit() {
 Die(<<END);
Usage: $0 [-c] [-n] [-v]
 -c to clear stored times (i.e. import all data)
 -n to prevent daemonising
 -v to be more verbose (reporting individual successful file imports)
END
}

die "$ROOT/ does not exist\n" unless -d $ROOT;

# daemonise and create notifier
Proc::Daemon::Init unless $opts{n};
Report("Started: PID=$$"); #do this after daemonising or it'll report the wrong PID
Report("Not reporting successful file imports (use -v option to change this)") unless $opts{v};
my $notifier = Linux::Inotify2->new() or Die("Unable to create notify object: $!");

my $ugid = join ':', (stat $ROOT)[4,5] if `whoami` =~ /^root$/; #if we can, change owner/group of new files to that of $ROOT

#subroutine called when something is created in the root directory (containing top-level directories)
my $topChange = sub {
 ScanTopDir($_[0]->fullname);
};

#subroutine called when something is created in a top level directory (containing project directories)
my $projectChange = sub {
 ScanProjDir($_[0]->fullname);
};

#subroutine called when something is created in a project directory (containing date directories)
my $dateChange = sub {
 ScanDateDir($_[0]->fullname);
};

#subroutine called when something is created in a date directory (containing material and metadata files)
my $fileChange = sub {
 Report("Imported " . $_[0]->fullname) if ImportFile($_[0]->fullname) && $opts{v};
 $_[0]->fullname =~ /(.*)\//;
 WriteTimestampFile($1, (stat $_[0]->fullname)[9]);
};

#initial scan of everything
Report("Watching $ROOT/ for new top-level subdirectories");
my $watch = $notifier->watch($ROOT, WATCH_MASK, $topChange) or Die("Couldn't watch: $!"); #do this before scanning or we could miss a top-level directory being created in the intervening period
Die("Couldn't open $ROOT/: $!") unless opendir ROOT, $ROOT;
foreach (readdir ROOT) {
   ScanTopDir("$ROOT/$_");
}
closedir ROOT;

#infinite loop waiting for stuff to appear
while (1) {
	$notifier->poll;
}

# subroutines that aren't pre-defined

sub ScanTopDir {
 my $topPath = shift;
 return unless -d $topPath;
 $topPath =~ m|$ROOT/(.*)|;
 return unless exists $topDirs{$1}; #only interested in certain top-level directories
 Report("Watching $topPath/ for new project subdirectories");
 my $watch = $notifier->watch($topPath, WATCH_MASK, $projectChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a date directory being created in the intervening period
 unless (opendir TOP, $topPath) {
    Warn("Couldn't open $topPath/: $!");
    $watch->cancel;
    return;
 }
 Report("Scanning $topPath/ for project subdirectories");
 foreach (readdir TOP) {
    ScanProjDir("$topPath/$_");
 }
 closedir TOP;
}

sub ScanProjDir {
 my $projPath = shift;
 return unless -d $projPath;
 return if $projPath =~ m|/\.\.?$|;
 Report("Watching $projPath/ for new date subdirectories");
 my $watch = $notifier->watch($projPath, WATCH_MASK, $dateChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a date directory being created in the intervening period
 unless (opendir PROJ, $projPath) {
    Warn("Couldn't open $projPath/: $!");
    $watch->cancel;
    return;
 }
 Report("Scanning $projPath/ for date subdirectories");
 foreach (readdir PROJ) {
    ScanDateDir("$projPath/$_");
 }
 closedir PROJ;
}

sub ScanDateDir {
 my $datePath = shift;
 return unless -d $datePath;
 return unless $datePath =~ /\/\d{8}$/;
 Report("Watching $datePath/ for new files");
 my $watch = $notifier->watch($datePath, WATCH_MASK, $fileChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a file being created in the intervening period
 unless (opendir DATE, $datePath) {
    Warn("Couldn't open $datePath/: $!");
    $watch->cancel;
    return;
 }
 my $timestamp = 0;
 if (!$opts{c} && -e "$datePath/$timestampFile") { #when this sub is called by a watcher, there will be no timestamp file anyway so $opts{c} check is irrelevant
    if (open TIMESTAMP, "$datePath/$timestampFile") {
       $timestamp = <TIMESTAMP>;
       chomp $timestamp;
       if (<TIMESTAMP> || $timestamp !~ /^\d+$/) {
          Warn("Unrecognised contents of $datePath/$timestampFile: re-importing all files in this directory\n");
          $timestamp = 0;
       }
       close TIMESTAMP;
    }
    else {
       Warn("Couldn't open $datePath/$timestampFile: $!: re-importing all files in this directory\n");
    }
 }
 Report("Scanning $datePath/ for files to import");
 my %files;
 my $mtime = 0; #in case no files
 foreach (readdir DATE) {
    my $file = "$datePath/$_";
    next if -d $file;
    $mtime = (stat "$datePath/$_")[9];
    next if $mtime < $timestamp; #if <=, may miss later files created in the same second; instead we may try to import the earlier files created in the same second twice on failure which isn't a major problem
    push @{$files{$mtime}}, $_;
 }
 closedir DATE;
 my $count = 0;
 foreach $mtime (sort keys %files) { #mtime order, oldest first
    foreach (@{$files{$mtime}}) {
       $count++ if ImportFile("$datePath/$_");
    }
 }
 Report('Imported ' . ($count ? $count : 'no') . ' file' . (1 == $count ? '' : 's') . " from $datePath/");
 WriteTimestampFile($datePath, $mtime);
}

sub ImportFile {
 my $imported = 1;
 my $fail = 0;
 if (!-d $_[0] && $_[0] =~ /\.[mM][xX][fF]$/) {
    $fail = system $mxfImport, $_[0];
 }
 elsif (!-d $_[0] && $_[0] =~ /\.[xX][mM][lL]$/) {
    $fail = system $xmlImport, $_[0];
 }
 else {
    $imported = 0;
 }
 if ($fail) {
    $_[0] =~ /(.*)\//;
    Warn("Importing $_[0] failed: $!. Will not retry unless daemon is re-run with -c option or after deleting $1/$timestampFile");
    $imported = 0;
 }
 return $imported;
}

sub WriteTimestampFile {
 my $file = "$_[0]/$timestampFile";
 if (!open TIMESTAMP, ">$file") {
    Warn("Couldn't open $file for writing: $!");
 }
 else {
    print TIMESTAMP "$_[1]\n";
    close TIMESTAMP;
    system 'chown', '-fh', $ugid, $file if $ugid; #if we can, change owner/group of file
 }
}

sub Die {
 syslog LOG_CRIT, (shift) . ": dying";
 closelog;
 exit 1;
}

sub Warn {
 syslog LOG_ERR, shift;
}

sub Report {
 syslog LOG_INFO, shift;
}
