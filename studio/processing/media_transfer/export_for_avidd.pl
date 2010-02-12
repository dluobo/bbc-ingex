#! /usr/bin/perl -w

#  $Id: export_for_avidd.pl,v 1.2 2010/02/12 14:51:05 john_f Exp $
#
# Copyright (C) 2009-10  British Broadcasting Corporation.
# All Rights Reserved.
# Authors: Matthew Marks, David Kirby
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

# If a subdirectory, <projDir>, of a directory <videoPath> is created or removed, where <videoPath> is a key of %videoRoots,
# this script creates or deletes path $ROOT/$SHARES_DIR/<projDir>$videoRoots{<videoPath>}/$SHARES_SUFFIX/
# and adds to or removes from file $SMB_CONF a share called <projDir>$videoRoots{<videoPath>} exported from $ROOT/$SHARES_DIR/<projDir>$videoRoots{<videoPath>}/

# If a subdirectory, <dateDir>, whose name consists of 8 digits, is created or removed in <videoPath>/<projDir>,
# this script creates or deletes symbolic link $ROOT/$SHARES_DIR/<projDir>$videoRoots{<videoPath>}/$SHARES_SUFFIX/$DEFAULT_HOST.<dateDir> which links to the <dateDir> subdirectory
# and will delete a link even if the $DEFAULT_HOST part of its name has been changed

# Initially scans the existing structure, applying the rules above to make sure that it and $SMB_CONF are correct

# If run as root, applies UID and GID of $ROOT to all created directories and links.  If not, it's unlikely to be able to update $SMB_CONF
# Applies permissions of $ROOT to all created directories

# Dies if directories $ROOT and those in %videoRoots do not initially exist, as it does not watch these for creation

# Logs to the system log (/var/log/messages)

use strict;
use Getopt::Std;
use Proc::Daemon;
use Linux::Inotify2;
use File::Path;
use File::Glob qw( :glob); # to ensure whitespace in names returned by glob is escaped
use Sys::Syslog qw(:standard :macros);

my $ROOT = '/store'; #the user/group of this is applied to all directories and links created if the script is run as root
my $SHARES_DIR = 'samba-exports'; #within $ROOT
my $SHARES_SUFFIX = 'Avid MediaFiles/MXF';
my $DEFAULT_HOST = 'INGEX';
my $SMB_CONF = '/etc/samba/smb.conf';
my $TEMP_SMB_CONF = '/tmp/smb-ingex.txt';

use constant WATCH_MASK => IN_CREATE | IN_MOVED_TO | IN_DELETE;
use constant CREATE => 1;
use constant ALREADY_EXISTS => 2;

# incoming directories to scan
my %videoRoots = (
      '/store/mxf_online', '_online',
      '/store/mxf_offline', '_offline',
);

openlog 'export_for_avidd', 'perror', 'user'; #'perror' echoes output to stderr (when script is run with -n)
# check arguments
my %opts;
&usage_exit unless getopts('n', \%opts);

sub usage_exit() {
 Die(<<END);
Usage: $0 [-n]
 -n to prevent daemonising
END
}

die "$ROOT/ does not exist\n" unless -d $ROOT;

# daemonise and create notifier

Proc::Daemon::Init unless $opts{n};
Report("Started: PID=$$"); #do this after daemonising or it'll report the wrong PID
my $notifier = Linux::Inotify2->new() or Die("Unable to create notify object: $!");

my $ugid = join ':', (stat $ROOT)[4,5] if `whoami` =~ /^root$/; #if we can, change owner/group of new dirs/links to that of $ROOT
my $perms = (stat $ROOT)[2] & 07777;

our %shareNames;
#subroutine called when a change happens in a video root directory (containing project directories)
my $projectChange = sub {
 my %links;
 if ($_[0]->IN_DELETE) { #an item has been removed which may be a project directory
    RmTree("$ROOT/$SHARES_DIR/" . ShareName($_[0]->fullname)) or Warn("Couldn't remove $ROOT/$SHARES_DIR/: $!"); #remove the corresponding shared path (including links) if present
    delete $shareNames{ShareName($_[0]->fullname)}; #remove the corresponding export if present
    UpdateConfFile();
 }
 elsif (ScanProjDir($_[0]->fullname, \%links) #a valid project dir
  && MkPath("$SHARES_DIR/" . ShareName($_[0]->fullname) . "/$SHARES_SUFFIX")) {
    MakeLinks(\%links);
    $shareNames{ShareName($_[0]->fullname)} = 1; #a valid project dir has an associated export path
    UpdateConfFile();
 }
};

#subroutine called when a change happens in a project directory (containing date directories)
our $dateChange = sub {
 if ($_[0]->IN_DELETE) { #an item has been removed which may be a date directory
    if ($_[0]->fullname =~ m|
     (.*)/ #root/project directory
     (\d{8})$ #date directory
     |x) { #a date directory
       #remove the links which should have been pointing at that directory
       foreach (glob "$ROOT/$SHARES_DIR/" . ShareName($1) . "/$SHARES_SUFFIX/*\\.$2") {
          if (-l $_) {
             Report("Removing redundant link $_ -> " . readlink $_);
             unlink "$_" or Warn("Couldn't remove $_: $!");
          }
       }
    }
 }
 else {
    AddLink($_[0]->fullname);
 }
};

#work out what shares and links are needed, based on the video tree, and watch the video tree for future changes
my %links; #source and targets for symlinks
foreach my $rootDir (keys %videoRoots) {
   if (-d $rootDir) {
      Report("Watching $rootDir/ for changes");
      my $watch = $notifier->watch($rootDir, WATCH_MASK, $projectChange) or Warn("Couldn't watch: $!"); #do this before scanning or we could miss a project directory being created in the intervening period
      unless (opendir ROOT, $rootDir) {
         Warn("Couldn't open $rootDir/: $!");
         $watch->cancel;
         next;
      }
      Report("Scanning $rootDir/ for project subdirectories");
      foreach my $projDir (readdir ROOT) {
         $shareNames{ShareName("$rootDir/$projDir")} = CREATE if ScanProjDir("$rootDir/$projDir", \%links); #a valid project dir has an associated export path; at this stage don't know whether the corresponding shared directory already exists or not
      }
      closedir ROOT;
   }
   else {
      Die("$rootDir/ is not a directory");
   }
}

#find existing exported directories and update %shareNames accordingly
my @toDelete;
if (-d "$ROOT/$SHARES_DIR/") {
   if (opendir SHARES, "$ROOT/$SHARES_DIR/") {
      foreach (readdir SHARES) {
         next if /^\.\.?$/;
         next unless -d "$ROOT/$SHARES_DIR/$_";
         if (exists $shareNames{$_}) { #there's a corresponding project directory
            $shareNames{$_} = ALREADY_EXISTS;
         }
         else {
            push @toDelete, $_;
         }
      }
   }
   else {
      Die("Couldn't open $ROOT/$SHARES_DIR/: $!");
   }
   closedir SHARES;
}
else {
   unlink "$ROOT/$SHARES_DIR"; #in case it's a plain file
   MkPath($SHARES_DIR) or Die('');
}

#delete redundant exported directories (with their links)
foreach (@toDelete) {
   RmTree("$ROOT/$SHARES_DIR/$_");
}

#now %shareName values are as follows:
# CREATE - video dir exists but shared dir doesn't
# ALREADY_EXISTS - video dir and shared dir both exist

#add new shared directories and check links in existing directories, updating %links accordingly
foreach my $shareName (keys %shareNames) {
   if (CREATE == $shareNames{$shareName}) {
      MkPath("$SHARES_DIR/$shareName/$SHARES_SUFFIX");
   }
   else { #already exists
      #check links
      my $path = "$ROOT/$SHARES_DIR/$shareName/$SHARES_SUFFIX";
      if (opendir LINKS, $path) { #there is a subdir of links
         #remove broken links and note existing ones (whose names may have been changed)
         foreach (readdir LINKS) {
            next unless -l "$path/$_";
            if (/(.+)\.(\d{8})$/ #correct syntax
             && exists $links{"$path/$DEFAULT_HOST.$2"} #link is required with this date
             && readlink "$path/$_" eq $links{"$path/$DEFAULT_HOST.$2"} #link points to the right place
            ) {
               Report("Leaving existing renamed link $path/$_") if $1 ne $DEFAULT_HOST;
               delete $links{"$path/$DEFAULT_HOST.$2"}; #don't need to create this
            }
            else {
               Report("Removing redundant link $path/$_ -> " . readlink "$path/$_");
               unlink "$path/$_" or Warn("Couldn't remove $path/$_: $!");
            }
         }
         closedir LINKS;
      }
      else {
         MkPath("$SHARES_DIR/$shareName/$SHARES_SUFFIX"); #in case it doesn't exist
      }
   }
}
MakeLinks(\%links);
UpdateConfFile();

while (1) {
   $notifier->poll;
}


sub ScanProjDir {
 my ($path, $links) = @_;
 my ($projDir) = $path =~ m|([^/]*)$|;
 return 0 if $projDir =~ /^\./;
 return 0 unless -d $path;
 Report("Watching $path/ for changes");
 my $watch = $notifier->watch($path, WATCH_MASK, $dateChange) or Warn("Couldn't watch: $!\n"); #do this before scanning or we could miss a date directory being created in the intervening period
 unless (opendir PROJ, $path) {
    Warn("Couldn't open $path/: $!");
    $watch->cancel;
    return 0;
 }
 Report("Scanning $path/ for date subdirectories");
 foreach my $dateDir (readdir PROJ) {
    AddLink("$path/$dateDir", $links);
 }
 closedir PROJ;
 return 1;
}

#creates a link, or puts the information in a hash, at a place based on the supplied path, targeting the suppled path
sub AddLink {
 my ($path, $links) = @_;
 my ($projDir, $dateDir) = $path =~ m|(.*)/(.*)$|;
 return unless $dateDir =~ /^\d{8}$/;
 return unless -d "$path";
 my %link = ("$ROOT/$SHARES_DIR/" . ShareName($projDir) . "/$SHARES_SUFFIX/$DEFAULT_HOST.$dateDir", $path); #default link name and link target
 if (defined $links) { #links hash supplied (as directories which will contain the links may not exist yet)
    #add the link to it
    %$links = (%link, %$links);
 }
 else {
    #make a link immediately
    MakeLinks(\%link);
 }
}

#returns the name of a share corresponding to a given project directory path
sub ShareName {
 $_[0] =~ m|
  (.*)/ #root directory
  (.*) #project directory
 |x;
 return "$2$videoRoots{$1}";
}

#creates symbolic links corresponding to the entries in the supplied hash
sub MakeLinks {
 foreach (keys %{ $_[0] }) {
    Report("Adding link $_ -> " . $_[0]->{$_});
    symlink $_[0]->{$_}, $_ or Warn("Couldn't create link $_: $!"); #path should already exist
    system 'chown', '-fh', $ugid, $_ if $ugid; #if we can, change owner/group of link to the same as its target
 }
}

#Adds new to, amd removes nonexistent and duplicated share sections from, the SAMBA configuration file, based on the keys in %shareNames
#If the file has changed, it is replaced atomically.
sub UpdateConfFile {
 my ($newConf, $lump, $sectionName, %found);
 my $hasChanged = 0;
 #copy over the parts we're keeping
 my $keepSection = 1;
 unless (open CONF, $SMB_CONF) {
    Warn("Can't read $SMB_CONF: $!");
    return;
 }
 my @conf = <CONF>; #reading all at once prevents "Use of uninitialized value in bitwise and (&) at /usr/lib/perl5/site_perl/5.8.8/i586-linux-thread-multi/Linux/Inotify2.pm line 277." when called by an inotify event!
 close CONF;
 foreach (@conf) {
     if (/^\s*\[(.+)\]/) { #a section heading
        #save previous section
        if ($keepSection && defined $lump) {
           $newConf .= $lump;
        }
        undef $lump;
        $sectionName = $1;
        $keepSection = 1;
     }
     $lump .= $_;
     if (m|^\s*path\s*=\s*$ROOT/$SHARES_DIR/(.*?)\s*$|) { #this share is one of ours (ignore trailing white space as per file format)
        if (exists $shareNames{$1} && !exists $found{$1}) { #this share is required and not already found
           #don't create it, and delete duplicate instances
           $found{$1} = 1;
        }
        else { #don't know about this share or it's not wanted
           #remove it
           Report("Removing " . (exists $shareNames{$1} ? "duplicated" : "redundant") . " share $sectionName from conf file") if defined $sectionName;
           $keepSection = 0;
           $hasChanged = 1;
        }
     }
 }
 if ($keepSection && defined $lump) {
    $newConf .= $lump;
 }
 $newConf =~ s/[\n\s]*$/\n/s; #remove trailing blank lines which can accumulate when sections are removed (but leave a final newline)
 #add any new shares
 foreach (keys %shareNames) {
    if (!exists $found{$_}) {
       Report("Adding new share $_ to conf file");
       $hasChanged = 1;
       $newConf .= <<END;

[$_]
	comment = Created by Ingex
	path = $ROOT/$SHARES_DIR/$_
	read only = No
	inherit acls = Yes
END
   }
 }
 #save if necessary
 if ($hasChanged) {
    unless (open CONF, ">$TEMP_SMB_CONF") {
       Warn("Can't open $TEMP_SMB_CONF for writing: $!");
       return;
    }
    print CONF $newConf;
    close CONF;
    Warn("Can't move $TEMP_SMB_CONF to $SMB_CONF: $!") if system '/bin/mv', '-f', $TEMP_SMB_CONF, $SMB_CONF; #change it atomically; perl's rename doesn't work across file systems so better to use mv
 }
}

#recursively creates a path under $ROOT, changing the user and group name of each created directory under $ROOT to that of $ROOT if running as root.  Assumes $ROOT already exists
sub MkPath {
 if (mkpath("$ROOT/$_[0]")) {
    Report("Created $ROOT/$_[0]/");
    my $path = $ROOT;
    foreach (split /\//, $_[0]) {
       $path .= "/$_";
       system 'chown', '-fh', $ugid, $path if $ugid; #...change owner/group of dir to that of $ROOT; don't use -R option as wasteful to change everything in the tree
       chmod $perms, $path;
    }
    return 1;
 }
 Warn("couldn't create $ROOT/$_[0]/: $!");
 return 0;
}

sub RmTree {
 if (rmtree($_[0])) {
    Report("Removed $_[0]/ and any contents");
    return 1;
 }
 Warn("couldn't remove $_[0]/: $!");
 return 0;
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
