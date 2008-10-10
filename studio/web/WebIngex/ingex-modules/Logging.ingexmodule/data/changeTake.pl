#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
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

use CGI::Pretty qw(:standard);

use lib ".";
use lib "..";
use lib "../../../ingex-config";
use ingexconfig;
use ingexhtmlutil;
use prodautodb;
use IngexJSON;
use ILutil;

print header;

my $dbh = prodautodb::connect(
	$ingexConfig{"db_host"},
	$ingexConfig{"db_name"},
	$ingexConfig{"db_user"},
	$ingexConfig{"db_password"}
) or die();

my $errorMessage;

if (($errorMessage = validate_params()) eq "ok")
{
	my $ok = "yes";

	my $take = prodautodb::load_take($dbh, param('id')) or $ok = "no";
	
	my $dataindex = uc(trim(param('dataindex')));
	my $unaltered_dataindex = $dataindex;
	
	my $value = trim(param('value'));
	
	if($dataindex eq "RESULT" && ($value eq "Good" || $value eq "good" || $value eq "GOOD" || $value eq "G" || $value eq "g")) {
		$value = 2;
		$dataindex = "RESULTID";
	} elsif ($dataindex eq "RESULT") {
		$value = 3;
		$dataindex = "RESULTID";
	}
	
	my $reclocerr = "";
	
	if($dataindex eq "LOCATION") {
		$dataindex = "LOCATIONID";
		my $recLocs = prodautodb::load_recording_locations($dbh) 
		    or $ok = "-1";
		
			my $recLocId;
            foreach my $recLoc (@{$recLocs})
            {
                if ($recLoc->{"NAME"} eq $value)
                {
                    $value = $recLoc->{"ID"};
					$recLocId = "done";
                    last;
                }
            }
            if (!defined $recLocId)
            {
               $ok = "-1";
				$reclocerr = "Could not match recording location to one in database.";
            }
	}
	
	if($ok ne "-1") {
		$take->{$dataindex} = $value; 

		my $takeid = prodautodb::update_take($dbh, $take) or $ok = "no";
		
		my $x = prodautodb::load_take($dbh, $takeid) or $ok = "no";

		if($ok eq "yes") {
			print '{"success":true,"error":"","value":"'.$x->{$unaltered_dataindex}.'"}';
		} else {
			my $err = $prodautodb::errstr;
			$err =~ s/"/\\"/g;
			print '{"success":false,"error":"'.$err.'"}';
		}
	} else {
		print '{"success":false,"error":"Problem resolving recording location. '.$reclocerr.'"}';
	}
} else {
	print '{"success":false,"error":"'.$errorMessage.'"}';
}
exit (0);

sub validate_params
{
	return "No data index defined" if (!defined param('dataindex') || param('dataindex') =~ /^\s*$/);
	return "No id defined" if (!defined param('id') || param('id') =~ /^\s*$/);

    return "ok";
}
