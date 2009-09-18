#!/usr/bin/perl -W

#
# $Id: media_transfer.pl,v 1.3 2009/09/18 17:05:47 philipn Exp $
#
# 
#
# Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
 
use strict;

use Getopt::Std;

use POSIX ":sys_wait_h";


# default settings

my $keepSuccessPeriod = 12 * 60 * 60; # 12 hours (in seconds)

my $successDirectory = "/video/mxf/";

my $destIncomingDirectory = "/store/video/incomming"; 
my $destLiveDirectory = "/store/video/";

my $useDateSubdirectory = 0;


# get options

sub print_usage
{
    printf "Usage: $0 <options>\n\n"
        . "Options:\n"
        . "    -h           Show this usage message\n"
        . "    -p <secs>    Time period (in seconds) before a file is deleted (default = $keepSuccessPeriod)\n"
        . "    -s <dir>     Success directory (default = '$successDirectory')\n"
        . "    -i <dir>     Destination incoming directory (default = '$destIncomingDirectory')\n"
        . "    -l <dir>     Destination live directory (default = '$destLiveDirectory')\n"
        . "    -d           Place file in live subdirectory with name equal to the date string in the filename\n"
        . "\n\n"
        . "The files are rsync-ed from the success directory to the destination\n"
        . "incoming directory, and then from there moved to the live directory \n"
        . "(or subdirectory when option -d is used).\n"
        . "Files that have been rsync-ed and which are older than the given time\n"
        . "period, will be deleted.\n";

    return 1;        
}

my %opts;
getopt('psil', \%opts) or (print_usage() && exit(1));

(print_usage() && exit(0)) if ($opts{"h"} or $opts{"-help"});

$keepSuccessPeriod = $opts{"p"} if ($opts{"p"});
$successDirectory = $opts{"s"} if ($opts{"s"});
$destIncomingDirectory = $opts{"i"} if ($opts{"i"});
$destLiveDirectory = $opts{"l"} if ($opts{"l"});
$useDateSubdirectory = 1 if ($opts{"d"});



# check the settings

die "invalid time period, $keepSuccessPeriod\n" if ($keepSuccessPeriod !~ /\d+/ || $keepSuccessPeriod < 0); 
die "success directory, '$successDirectory', does not exist\n" unless (-e $successDirectory);
die "destination incoming directory, '$destIncomingDirectory', does not exist\n" unless (-e $destIncomingDirectory);
die "destination live directory, '$destLiveDirectory', does not exist\n" unless (-e $destLiveDirectory);



# get time now for comparison later with last modified time to determine if
# sucessfully transferred files should be deleted
my $now = time;



# get list of files from the success directory

my @essenceFiles;

opendir(SUCCESS, $successDirectory) or die "failed to open dir '$successDirectory': $!";

while (my $fname = readdir(SUCCESS))
{
    # accept files ending with .mxf
    if ($fname =~ /\.[mM][xX][fF]$/) 
    {
        my $fullPath = $successDirectory . "/" . $fname;
        
        my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
                $blksize,$blocks) = 
            lstat($fullPath);
        if ($dev) # if lstat failed then is returns a null list
        {
            push(@essenceFiles,
                {
                    fname => $fname,
                    fullpath => $fullPath,
                    mtime => $mtime,
                    size => $size
                }
            );
        }
        else
        {
            print "failed to lstat file '$fullPath': $!\n" 
                . "skipping file and continuing processing\n";
        }
    }
}

closedir(SUCCESS);



# process files in order of last modified time

foreach my $essenceFile (sort { $a->{"mtime"} <=> $b->{"mtime"} } @essenceFiles)
{
    my $fileInDestIncoming = $destIncomingDirectory . "/" . $essenceFile->{"fname"};
    my $fileInDestLive;
    my $fileInDestLiveSubDirectory = undef;
    if ($useDateSubdirectory && $essenceFile->{"fname"} =~ /\d{8}[^\/]*$/)
    {
        $fileInDestLiveSubDirectory = $destLiveDirectory 
            . "/"
            . ($essenceFile->{"fname"} =~ /(\d{8})[^\/]*$/)[0];
        $fileInDestLive = $fileInDestLiveSubDirectory 
            . "/"
            . $essenceFile->{"fname"};
    }
    else
    {
        $fileInDestLive = $destLiveDirectory . "/" . $essenceFile->{"fname"};
    }

    
    if (-e $fileInDestLive) # file exists in the live directory
    {
        # warn and skip if the file size differs (possibly due to filename clash in system)
        my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
                $blksize,$blocks) = 
            lstat($fileInDestLive);
        if (!defined $dev)
        {
            print "failed to lstat file '$fileInDestLive' to check size equals "
                ."'$essenceFile->{'fullpath'}': $!\n" 
                . "skipping file and continuing processing\n";
        }
        elsif ($size != $essenceFile->{"size"})
        {
            print "File '$essenceFile->{'fname'}' in destination live directory has "
                . "different size ($size) than file in source " 
                . "success directory ($essenceFile->{'size'})\n"
                . "possible file name clash in system\n"
                . "skipping file and continuing processing\n";
        }
        elsif ($essenceFile->{"mtime"} + $keepSuccessPeriod < $now)
        {
            # remove old file
            print "removing old file '$essenceFile->{'fullpath'}'\n"; 
            unlink($essenceFile->{"fullpath"})
                or print "failed to remove '$essenceFile->{'fullpath'}': $!\n"
                    . "continuing processing\n";
        }
    }
    else
    {
        print "rsyncing file '$essenceFile->{'fullpath'}' to '$destIncomingDirectory'\n";

        {
            my $rsyncPid;
            
            # setup the signal handlers that will be restored
            # after this block is exited
            local $SIG{"INT"} = sub { kill("INT", $rsyncPid); };
            local $SIG{"TERM"} = sub { kill("TERM", $rsyncPid); };
            
            # fork rsync child        
            if (!defined ($rsyncPid = fork()))
            {
                die "failed to fork: $!";
            }
            elsif ($rsyncPid == 0) # child
            {
                exec("rsync", 
                    "-t", # preserve times
                    "-P",
                    $essenceFile->{"fullpath"}, # source 
                    $destIncomingDirectory # destination
                );
                die "failed to exec rsync: $!";
            }
    
            # wait for rsync child
            waitpid($rsyncPid, 0);
        }
        
        # check exit of wait call and rsync exec
        if ($? == -1) 
        {
            # rsync failed
            print "failed to execute rsync: $!\n";
            print "skipping file and continuing processing\n";
        }
        elsif ($? & 127) 
        {
            # child has received signal to terminate so we terminate
            printf "rsync child died with signal %d\n", ($? & 127);
            print "exiting\n";
            exit(1); # TODO: use more exit code values
        }
        elsif ($? >> 8)
        {
            printf "rsync failed, exited with value %d\n", $? >> 8;
            if ($? >> 8 == 20) # 20 means rsync received SIGUSR1 or SIGINT
            {
                # child has received signal to terminate so we terminate
                print "rsync received SIGUSR1 or SIGINT signal\n";
                print "exiting\n";
                exit(1); # TODO: use more exit code values
            }
            # else we continue with the next file
            print "skipping file and continuing processing\n";
        }
        else
        {
            # make live subdirectory if it doesn't yet exist
            if ($fileInDestLiveSubDirectory)
            {
                if (-e $fileInDestLiveSubDirectory && !(-d $fileInDestLiveSubDirectory))
                {
                    # it is not a directory
                    print "Existing file conflicts with live subdirectory $fileInDestLiveSubDirectory\n";
                    exit(1);
                }
                
                mkdir $fileInDestLiveSubDirectory, 0777;
            }
            
            # move the file from the incoming directory to the live directory
            print "moving file '$fileInDestIncoming' to '$fileInDestLive'\n"; 
            if (!rename($fileInDestIncoming, $fileInDestLive))
            {
                print "failed to move file from '$fileInDestIncoming' to "
                    . "'$fileInDestLive': $!\n"
                    . "continuing processing\n";
            }
            elsif ($essenceFile->{"mtime"} + $keepSuccessPeriod < $now)
            {
                # remove old file
                print "removing old file '$essenceFile->{'fullpath'}'\n"; 
                unlink($essenceFile->{"fullpath"})
                    or print "failed to remove '$essenceFile->{'fullpath'}': $!\n"
                        . "continuing processing\n";
            }
        }        
    }
}




