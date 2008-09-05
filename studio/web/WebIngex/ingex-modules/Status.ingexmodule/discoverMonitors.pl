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
use lib "../../ingex-config";
use ingexconfig;
use ingexhtmlutil;
use prodautodb;
use IngexStatus;

print header;

my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();

my $dirtoget='./monitors/';
opendir(MONITORS, $dirtoget) or returnError();
my @thefiles = readdir(MONITORS);
closedir(MONITORS);

my $f;
my %monitors;
my $nodes;
my %data;

foreach $f (@thefiles) {
	if(substr($f, -16) eq '.ingexmonitor.pl'){
		substr($f, -16)  = '';
		$monitors{$f} = getMonitor($f);
	}
}

$nodes = getNodes();

$data{'monitors'} = \%monitors;
$data{'nodes'} = $nodes;
print hashToJSON(%data);

exit(0);

sub returnError
{
	exit(0);
}

sub getMonitor
{
	my $m = $_[0];	
	my $retval = "json:";
	$ENV{"PATH"} = "";
	if ($m =~ /(\w*)/) {
	    $retval .= `./monitors/$1.ingexmonitor.pl monitorInfo`;
	}
	return $retval;
}

sub getNodes
{
	my %nodes;
	my %n;
	my $nodesArray = load_nodes($dbh) 
	    or return_error_page("failed to load nodes: $prodautodb::errstr");
	
	foreach my $node (@$nodesArray) {
		%n = %$node;
		$nodes{$n{'NAME'}} = {nodeType=>$n{'TYPE'},ip=>$n{'IP'},volumes=>$n{'VOLUMES'}};
	}
	
	return \%nodes;
}