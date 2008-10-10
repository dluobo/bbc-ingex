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

	my $comment = "";
	if (defined param('comment')) { $comment = trim(param('comment')); }
	
	my $editratedenom = 1;
	if (defined param('editratedenom')) {
		$editratedenom = param('editratedenom');
	}
	
	my $resultID = 1;
	if(param('result') eq "No Good") {
		$resultID = 3;
	} elsif (param('result') eq "Good") {
		$resultID = 2;
	}

	my $take = {
		TAKENO => param('takeno'),
		LOCATION => param('location'),
		DATE => param('date'),
		START => param('start'),
		LENGTH => param('length'),
		RESULT => $resultID,
		COMMENT => $comment,
		ITEM => param('item'),
		EDITRATE => param('editrate'),
		EDITRATEDENOM => $editratedenom
	};

	my $takeid;
	$takeid = prodautodb::save_take($dbh, $take) or $ok = "no";

	my $loadedTake = prodautodb::load_take($dbh, $takeid) or $ok = "no";

	if($ok eq "yes") {
		print '{"success":true,"error":"","id":'.$takeid.'}';
	} else {
		my $err = $prodautodb::errstr;
		$err =~ s/"/\\"/g;
		print '{"success":false,"error":"'.$err.'"}';
	}
} else {
	print '{"success":false,"error":"'.$errorMessage.'","id":-1}';
}
exit (0);

sub validate_params
{
    return "No take number defined" if (!defined param('takeno') || param('takeno') =~ /^\s*$/);
	return "No item defined" if (!defined param('item') || param('item') =~ /^\s*$/);
	return "No location defined" if (!defined param('location') || param('location') =~ /^\s*$/);
	return "No date defined" if (!defined param('date') || param('date') =~ /^\s*$/);
	return "No start timecode defined" if (!defined param('start') || param('start') =~ /^\s*$/);
	return "No duration defined" if (!defined param('length') || param('length') =~ /^\s*$/);
	return "No result (good/no good) defined" if (!defined param('result') || param('result') =~ /^\s*$/);

    return "ok";
}
