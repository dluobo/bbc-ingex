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
use IngexJSON;
use DBI qw(:sql_types);

print header;

my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();

my %data;
$data{'recLocs'} = getRecLocs();
$data{'series'} = getSeries();

print hashToJSON(%data);

exit(0);

sub getRecLocs
{
	my $recLocs = prodautodb::load_recording_locations($dbh) or 
	    return_error_page("failed to load recorder locations: $prodautodb::errstr");
	
	my %recLocNames;
	
	foreach my $rcl (@{$recLocs})
    {    
        $recLocNames{$rcl->{'ID'}} = $rcl->{'NAME'};
    }

	return \%recLocNames;
}

sub getSeries
{
	my $series = load_series($dbh) or 
	    return_error_page("failed to load recorder locations: $prodautodb::errstr");
	
	my %seriesList;
	my $s;
	
	foreach $s (@{$series})
    {
		my $yy;
		$yy->{'name'} = $s->{'NAME'};
		$yy->{'programmes'} = load_series_progs($s->{'ID'},$s->{'NAME'},$dbh);
        $seriesList{$s->{'ID'}} = $yy;
    }

	return \%seriesList;
}

sub load_series
{
    my ($dbh) = @_;
    
    my $xx;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT srs_identifier AS id, 
                srs_name AS name
            FROM Series
            ");
        $sth->execute;
        
        $xx = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $xx;    
}

sub load_series_progs
{
    my ($id,$name,$dbh) = @_;
	my $yy;
	
	
    eval
    {
        my $sth = $dbh->prepare("
            SELECT prg_identifier AS id, 
                prg_name AS name
            FROM Programme
				WHERE prg_series_id = ?
            ");
		$sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $yy = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }

	my %progs;
	for my $prog (@$yy) {
		$progs{$prog->{'ID'}} =  $prog->{'NAME'};
	}
    return \%progs;
}