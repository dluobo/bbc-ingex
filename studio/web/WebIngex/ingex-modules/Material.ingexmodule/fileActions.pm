# Copyright (C) 2009  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
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


use strict;
use warnings;


use lib "../../ingex-config";
use lib "../../ingex-modules/Material.ingexmodule";

use db;
use ingexconfig;
use materialconfig;

use DBI qw(:sql_types);
use File::Temp qw(mktemp tempfile); 

my $debug = '';


my $localError;
my @deleteScript;
my $dbh;
my $line = 0; 	
my $query = '';
my @errors;

    
#
# delete packages that match supplied package ids
# return output of delete command
#
sub delete_packages
{
	my ($params) = @_;
	
	my @toDelete = @$params;
	my $param = '';
	
	$param = join(",", @toDelete);
	
	($param) = $param =~ /([\d,]+)|/;	# untaint so only CSV allowed

	my $resultsFilename = mktemp("/tmp/deletepkgresultsXXXXXX");
	my $cmd = join(" ",
                "deletepkg", 
				$param,
				">$resultsFilename"
        );


	$ENV{"PATH"} = $ingexConfig{"delete_pkg_dir"};
	$ENV{"LD_LIBRARY_PATH"} = $ingexConfig{"delete_pkg_dir"};
  	
    my $out = system($cmd);
        

    $out == 0 or 
      	del_error("Failed to delete packages. Using deletepkg in location: $ingexConfig{'delete_pkg_dir'}. Using command: $cmd.");
        
        
    # extract the results
    open(DELRESULTS, "<", "$resultsFilename") or
      	del_error("failed to open results file: $!"); 
    
    
    my $results = '';
    
    while (my $line = <DELRESULTS>)
        {
            chomp($line);
            
            $results .= $line."<br>";
        }
        
    close(DELRESULTS);
    unlink($resultsFilename); # clean up
        
    
	# all ok
	return $results;
}


#
# error deleting packages
#
sub del_error{
	my ($errorStr) = @_;
	die $errorStr;
}

1;