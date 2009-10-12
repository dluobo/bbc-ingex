#! /usr/bin/perl -w

#  $Id: export_for_avid.pl,v 1.1 2009/10/12 17:48:29 john_f Exp $
#
# Copyright (C) 2009  British Broadcasting Corporation.
# All Rights Reserved.
# Author: David Kirby
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


use File::Path;
use File::Glob qw( :glob); # to ensure whitespace in names returned by glob is escaped
use File::Find;

use Getopt::Std;

use strict;

our ($opt_v) = 0; # verbose ?
our ($opt_q) = 0; # quiet - suppress messages
our ($opt_h) = 0; # help
our ($opt_o) = ''; # output filename (used instead of smb.conf)
our ($opt_p) = ''; # avid directory prefix, i.e. host name of indexing Avid
getopts( 'ho:p:qt:v' ) or usage();
usage() if $opt_h;

sub usage {
   print <<END_USAGE;
$0 [options] <directory project-suffix> [<directory project-suffix>]
Options are:
   -h         help
   -o <file>  write the revised smb.conf file to <file> rather than updating the active file
   -p <host>  use <host> as the prefix for the shared media directories
   -q         suppress messages
   -v         verbose
END_USAGE
exit;
}

my ($dest_root_dir) = '/store/samba-exports';
#$dest_root_dir = '/home/david/samba-exports'; # for testing locally
my ($dest_dir_suffix) = 'Avid MediaFiles/MXF';
my ($default_project) = 'avid'; # if no project name given, use this

my ($smbConfFile) = '/etc/samba/smb.conf';

my (@video_dirs);    # incoming directories to scan  

#=for fixed directories that you always want to scan, use this section of code
@video_dirs = (
   {
      SRC_DIR          => '/store/mxf_online',
      DIR_SUFFIX     => '_online',
   },
   {
      SRC_DIR          => '/store/mxf_offline',
      DIR_SUFFIX     => '_offline',
   },
);
#=cut


while (@ARGV >= 2) {
  # get directory to scan and the project suffix to use for the samba export
  push @video_dirs, {SRC_DIR => shift(@ARGV), DIR_SUFFIX => '_' . shift(@ARGV)};
}
if (@ARGV) {
  print STDERR "Warning: missing project suffix for $ARGV[0]\n";
}

if (! scalar @video_dirs) {
  exit; # nothing to do
}

my ($tempFile) = '/tmp/smb-ingex.txt';
my ($updateSMBconfFile) = 0;  # set to 1 if we're to update the smb.conf file

my ($newSMBconf) = $opt_o;   # get output file name, if given
if ($newSMBconf) {
   if ($newSMBconf eq '-') {
      *NEW_SMB_CONF = *STDOUT;
   }
   else {
      open (NEW_SMB_CONF, '>', $newSMBconf) or die "Can't create file $newSMBconf\n";
   }
}
elsif (-w $smbConfFile) {
   $newSMBconf = $tempFile;
   $updateSMBconfFile = 1; # allow smb.conf to be updated
   open (NEW_SMB_CONF, '>', $newSMBconf) or die "Can't create file $newSMBconf\n";
}

my ($host) = $opt_p ? $opt_p : 'INGEX';
my ($hostPrefix) = uc($host) . '.';

=for testing
@video_dirs = (
   {
      SRC_DIR =>'/home/david/video/mxf_online',
      DEF_PROJECT => 'avid',
      DIR_SUFFIX => '_online',
   },
   {
      SRC_DIR =>'/home/david/video/mxf_offline',
      DEF_PROJECT => 'avid',
      DIR_SUFFIX =>'_offline',
   },
);
=cut

my ($sharesChanged) = 0;
my ($src_dir, $suffix);
my (@dirs, $videoDir);

# check if we're running as root, in which case we'll need to change ownership of the links we create
#my ($iAmRoot) = (getlogin || getpwuid($<)) eq 'root'; #doesn't work for sudo etc
my ($iAmRoot) = `whoami` =~ '^root$';

my ($project, $uid, $gid);

mkpath([$dest_root_dir]); # ensure that the root directory for the exports exists 

# ----- First find links in $dest_root_dir that no longer point to anything and remove them -----
if (opendir (DEST_DIR, $dest_root_dir) ) {
  @dirs = readdir DEST_DIR;
  close DEST_DIR;

EXPORTED_DIR:
  foreach (@dirs) { # e.g. project_online
    next if /^\.{1,2}/;
    #print "Checking exported dir $_\n";

    # Check that the individual links from the exported directory are still valid
    $opt_v && print "Checking for stale links under $dest_root_dir/$_\n";   
 
    foreach my $exportedProjectDate (glob "$dest_root_dir/$_/$dest_dir_suffix/*") {
      if (! -e $exportedProjectDate) { # check that the target for this link still exists
        $opt_v && print "Deleting $exportedProjectDate as its target no longer exists\n";
        rmtree ("$exportedProjectDate", {verbose=>0, safe=>1 } );
      }
    }
    # Check for projects that no longer exist and remove the links to them
    foreach  $videoDir (@video_dirs) {
      #print "Checking against $videoDir->{DIR_SUFFIX}\n";
      ($project) = /^(.*)$videoDir->{DIR_SUFFIX}$/;
      if ($project) { # did the directory name match this suffix?
        #print "Matched project name $project\n";
        # get source directory and see if a directory with this name is there
        # remove suffix from directory name e.g. block1234_online
        if ($project eq $default_project ) {
          $project = '';
          #print "Skipping default project\n";
          # if default project name found here, we need to check that a numbered directory exists in the source directory to see if it's still in use
          if (! grep ( /\/\d{8,8}$/, glob "$videoDir->{SRC_DIR}/*")  ){
            # a date directory at the top level wasn't found, so delete this link
            if (-e "$dest_root_dir/$_/$dest_dir_suffix") { 
              # seems to be one of our video directories as expected, but source dir has gone, so go ahead and delete it
              $opt_v && print "$videoDir->{SRC_DIR}/<date> doesn't exist - deleting target: $dest_root_dir/$_\n";
              rmtree ("$dest_root_dir/$_", {verbose=>0, safe=>1,});
            }
          }
        }
        elsif (! -e "$videoDir->{SRC_DIR}/$project") {
          # project has gone from source directory, so check if we should remove it from the samba exports.
          # See if the full path with $dest_dir_suffix exists, to check that this really is supposed 
          # to be linked back to video content in the source directory area
          #print "Source dir missing\n";
          if (-e "$dest_root_dir/$_/$dest_dir_suffix") { 
            # seems to be one of our video directories as expected, but source dir has gone, so delete it
            $opt_v && print "$videoDir->{SRC_DIR}/$project doesn't exist - deleting target: $dest_root_dir/$_\n";
            rmtree ("$dest_root_dir/$_", {verbose=>0, safe=>1});
          }
        }
        next EXPORTED_DIR; 
      }
    }
  }
}
else { 
   die "Exiting - could not read directory $dest_root_dir \n";
}

# --- Look for either <project_name>/<date> or just <date> directories in the source directory areas, and make links to them ---
# For example: /video/mxf_online/projectName/20090216
# will be linked to from /samba-exports/<projectName_online>/Avid MediaFiles/MXF/INGEX.20090216
# and then /samba-exports/<projectName_online> will be added as a samba share under the name <projectName_online>
# You can then map this drive from a PC and Avid will be able to access the content.
# Note that for an Avid system to index the directory you need to change the INGEX. prefix to the host name 
# of you PC (in uppercase), or remove the prefix completely.

foreach $videoDir (@video_dirs) {
   my $src_dir = $videoDir->{SRC_DIR};
   my $suffix = $videoDir->{DIR_SUFFIX};
   my ($dirName, $dest_dir, $new_project_dir, $existingDir, $newLinkedDir);
   $opt_v && print "Examining projects in $src_dir\n";

   @dirs = glob("$src_dir/*");
DIR:
   foreach (@dirs) {
      if (! -d $_ ) { next; };
      if (/^\.{1,2}/) { next; } # skip . and .. and hidden dirs
      #print "Found $_\n";

      ($dirName) = /([^\/]*)$/;
      $opt_v && print "Checking project $dirName\n";
      # Look for top-level date directories
      if ($dirName =~ /^\d{8,8}$/) {
         # 8-digit directory name found
         # check if the target directory already exists but with a different host name prefix
         $new_project_dir = $dest_root_dir . '/' . $default_project . $suffix;
         $dest_dir = $new_project_dir . '/' . $dest_dir_suffix;
         ($uid, $gid) = (stat _) [4,5];
         foreach $existingDir (glob "$dest_dir/*" ){
            $existingDir =~ /(\d{8,8})$/ ;
            if ($1 && ($1 =~ /$dirName$/)) {
               $opt_v && print "Skipping $dirName - already linked from $existingDir\n";
               next DIR;
            }
         }
         $newLinkedDir = $dest_dir . '/' . $hostPrefix . $dirName;
         if (! -e $newLinkedDir) {
            $opt_q or print "Linking to $_ from $newLinkedDir\n";
            mkpath ([$dest_dir]);
            symlink($_, $newLinkedDir);
            if ($iAmRoot && $uid && $gid) { # set uid and gid to the owner of the source file to which we're linking
              # find ( sub {print "chown $_\n"; chown ($uid, $gid, ($_) )}, ($dest_dir) );
               $uid .= '.' . $gid;
               system ("chown -fR $uid $new_project_dir");
            }
         }
         next;
      }

      # This directory may be a video subdirectory - see if it contains 8-digit date directories
      $project = $dirName; # this will be the project directory
      my (@subDirs) = glob ("$_/*");
VDIR:
      foreach my $subD (@subDirs) {
         if (! -d $subD ) { next; };
         if ($subD =~ /^\.{1,2}/) { next; } # skip . and .. and hidden dirs
         $opt_v && print "Found date directory $subD\n";

         ($dirName) = $subD =~ /([^\/]*)$/;
         #print "this dir is $thisDir\n";
         if ($dirName =~ /^\d{8,8}$/) {
            # 8 digits found
            #$opt_v && print "Found $subD\n";
            $new_project_dir = $dest_root_dir . '/' . $project . $suffix;
            $dest_dir = $new_project_dir . '/' . $dest_dir_suffix;
            ($uid, $gid) = (stat _) [4,5]; # picked up from glob above
            # check if the directory already exists but with a different host name prefix
            foreach $existingDir (glob "$dest_dir/*" ){
               $existingDir =~ /(\d{8,8})$/ ;
               if ($1 && ($1 =~ /$dirName$/) ) {
                  $opt_v && print "Skipping $dirName - already linked from $existingDir\n";
                  next VDIR;
               }
            }

            $newLinkedDir = $dest_dir . '/' . $hostPrefix . $dirName;
            if (! -e $newLinkedDir) {
               $opt_q or print "Linking to $subD from  $newLinkedDir\n";
               mkpath ([$dest_dir]);
               symlink ($subD,  $newLinkedDir);
               if ($iAmRoot && $uid && $gid) {
                  #find ( sub {chown ($uid, $gid, ($_) )}, $dest_dir );
                  $uid .= '.' . $gid;
                  system ("chown -fR $uid $new_project_dir");
               }
            }
         }
      }
   }
}

# ------ Create samba shares for the directories in $dest_root_dir  ------

if ($newSMBconf) {
  # create SAMBA exports for the projects
   
  # open /etc/samba/smb/conf
  # copy it to temp file, line by line until we see [projectname], then create a hash 'projectname' to show it already exists
  # Append the shares that haven't already been found
  # if any share added, and we have write access, mv the temp file to smb.conf
  
  open SMB_CONF, $smbConfFile or die "Can't read $smbConfFile\n";
  #open NEW_SMB_CONF, ">$tempFile" or die "Can't create temp file $tempFile\n";
  my (@section) = ();
  my (%sectionName) ;
  my (@newSMBconf) = ();
  my ($keepShare) = 1;
  my ($path);
  $sharesChanged = 0; # reset as we may not actually need to change the samba conf file
  
  # Check to see if there are shares that no longer point to a valid path (i.e. projects that may have been deleted)
  while (<SMB_CONF>) {
    if (/^\s*\[(.+)\]/) {
      # found a section
      #print "Found section $1\n";
      if (@section && $keepShare) {
        #print "Keeping previous section\n";
        push @newSMBconf, @section;
        #print NEW_SMB_CONF @section; # save the previous section
      }
      @section = ($_); # start new section
      #print "Gathering lines for section $_\n";
      $keepShare = 1;
      $sectionName{$1} = 1; # make a hash 
    }
    elsif (@section) {
      # save entries in this section and check if it's one of our samba exports and that 
      # the source directory still exists
      push @section, $_;
      if (/^\s*path\s*=/ ) {
        # is this one of our shares?
        ($path) = /=\s*(.*)/;
        #print "Found path  $path\n";
        if ($path =~ /^$dest_root_dir/) {
          # does the target for the link still exist?
          # print "Looks like one of ours - checking it still exists\n";
          if (! -e $path) {
            $opt_v && print "Removing SAMBA share as $path no longer exists\n";
            $keepShare = 0;
            $sharesChanged = 1;
          }
        }
      }
    }
    else {
      # keep this line (probably a comment at start of file)
      push @newSMBconf, $_;
    }
  }
  if (@section && $keepShare) {
    push @newSMBconf, @section; # save the previous section
  }
  close SMB_CONF;
   
  # avoid leaving blank lines at the end otherwise the smb.conf file can grow as we add and remove shares
  my ($n) = $#newSMBconf; # if empty = -1
  while ($n > 0 && ($newSMBconf[$n] =~ /^\s*$/  ) ) {
    $n--;
  }
  $#newSMBconf = $n;
  
  print NEW_SMB_CONF @newSMBconf; # save the revised samba config
  
  # Now add any new shares
  foreach my $sambaExport (glob "$dest_root_dir/*") {
    if (! -d $sambaExport ) { next; }
    if ($sambaExport =~ /^\.{1,2}/) { next; } # skip . and .. and hidden dirs
   
    my  ($shareName) = $sambaExport =~ /([^\/]+)$/;
    if ($shareName) {
      if (! exists $sectionName{$shareName} ) {
        $sharesChanged = 1;
        $opt_v && print "Adding new share: $shareName\n";
        print NEW_SMB_CONF "\n[", $shareName, "]\n   comment = Created by Ingex\n";
        print NEW_SMB_CONF "   path = $sambaExport/\n   read only = No\n   inherit acls = Yes\n";
      }
    }
  }
  close NEW_SMB_CONF;

  if ($updateSMBconfFile && $sharesChanged) { 
    #print "Renaming $tempFile to $smbConfFile\n";
    rename $newSMBconf, $smbConfFile or unlink $newSMBconf;
  }
}
