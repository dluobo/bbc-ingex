#!/usr/bin/perl -wT

# Copyright (C) 2009  British Broadcasting Corporation
# Author: Sean Casey
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

use strict;
use CGI qw(:standard);

use lib ".";
use lib "../../ingex-config";
use ingexhtmlutil;

my $fname = param("fileIn");
my $outname;

# extract file name from path
my $endpath = rindex($fname,"/");

if($endpath > -1){
    $outname = substr $fname, $endpath+1; # strip path
}
else{
    $outname = $fname;
}


if($fname eq ''){
    return_error_page("No filename supplied");
}

open FILE, $fname or die $!;
my @data = <FILE>;
close FILE;

print "Content-Type:application/x-download\n";
print "Content-Disposition:attachment;filename=$outname\n\n";
print @data;
