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

	my $itemid;
	$itemid = prodautodb::delete_item($dbh, param('id')) or $ok = "no";

	if($ok eq "yes") {
		print '{"success":true,"error":""}';
	} else {
		my $err = $prodautodb::errstr;
		$err =~ s/"/\\"/g;
		print '{"success":false,"error":"'.$err.'"}';
	}
} else {
	print '{"success":false,"error":"'.$errorMessage.'"}';
}
exit (0);

sub validate_params
{
	return "No item database id defined" if (!defined param('id') || param('id') =~ /^\s*$/);

    return "ok";
}
