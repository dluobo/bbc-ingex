#!/usr/bin/perl -wT

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

##########################################################################################
#
# JSON tree loader class - passes json tree data back to Ext tree as nodes are expanded
#
##########################################################################################

use strict;
use warnings;

use lib "../ingex-config";
use lib "..";
use lib ".";

use db;
use ingexconfig;

use Time::Local;
use Time::Local 'timelocal_nocheck';

use JSON::XS;

my @t_data;

use CGI qw(:standard);

my $dbh = prodautodb::connect(
		$ingexConfig{"db_host"}, 
		$ingexConfig{"db_name"},
		$ingexConfig{"db_user"}, 
		$ingexConfig{"db_password"})
 	or die();

my $level = 0;
my @params;
my $filterOpts;

my $videoFormat = param("formatIn");
my $startTs  	= param("tStartIn");	 # stored as time stamp
my $endTs    	= param("tEndIn");
my $range       = param("rangeIn");
my $day         = param("dayIn");
my $time        = param("timeIn");
my $date        = param("dateIn");
my $projectId   = param("projectIn");
my $keywords    = param("keywordsIn");    # key search terms

$filterOpts->{'video'}     		= $videoFormat;
$filterOpts->{'keywords'}  		= $keywords;
$filterOpts->{'startTimestamp'} = $startTs;
$filterOpts->{'endTimestamp'}   = $endTs;
$filterOpts->{'range'}     		= $range;
$filterOpts->{'day'}       		= $day;
$filterOpts->{'time'}      		= $time;
$filterOpts->{'date'}      		= $date;
$filterOpts->{'projectid'} 		= $projectId;


# if time set time period has been supplied, convert this to start and end times
if ( $filterOpts->{'range'} && $filterOpts->{'day'} ) {
	my $converted = rangeToTimestamp( $filterOpts->{'range'}, $filterOpts->{'day'} );
	$filterOpts->{'startTimestamp'} = $converted->{'startTimestamp'};
	$filterOpts->{'endTimestamp'} = $converted->{'endTimestamp'};
}


################################################################
#
# different data for different heirachy levels
# project_name->date->timecode->materials
#
################################################################

if ( param("countIn") ) {                  # only return count of materials
	my $count;
	my $date = $filterOpts->{'date'};
	
	eval{
		$count = db::count_materials( $dbh, $filterOpts );
	};
	if($@){
		loader_error("err~$@");
	}
	else{
		@t_data = ( { count => $count }, { date => $date } );
	}
}

# get node depth
if ( !param("countIn") ) {
	my $data = param("node");
	@params = split( /,/, $data );
	$level  = $params[0];                 
	$level++;
}
	
#
# 1: project name level
#
if ( $level == 1 ) {
	my $projects;
	if ( $filterOpts->{'projectid'} < 0 ) {
		$projects = db::load_projects($dbh);
	}
	else {
		$projects = db::load_projects( $dbh, $projectId );
	}

	if ( defined $projects ) {
		foreach my $project ( @{$projects} ) {
			$filterOpts->{'projectid'} = $project->{'ID'};
			my @ids = db::get_material_ids_a( $dbh, $filterOpts );
			my $count = @ids;
			my $ids = db::arr_to_csv(@ids);
					
			if ( $count > 0 )	# ensure materials exist on this branch
			{    

				my $id = "1," . $project->{'ID'};

				if ( param("noDataAndCountIn") ) {
					# do not add a count of the materials on this branch

					push(
						@t_data,
						{
							name       => $project->{'NAME'},
							uiProvider => 'Ext.tree.ColumnNodeUI',
							id         => $id,
							projId     => $project->{'ID'}
						}
					);
				}
				else {
					
					push(
						@t_data,
						{
							name          => $project->{'NAME'},
							uiProvider    => 'Ext.tree.ColumnNodeUI',
							id            => $id,
							projId        => $project->{'ID'},
							materialCount => $count,
							materialIds   => $ids
						}
					);
				}

			}

		}
	}

}

#
# 2: date level
#
elsif ( $level == 2 ) {
	my @dates = db::get_unique_dates( $dbh, $params[1] );

	foreach my $date (@dates) {
		$filterOpts->{'projectid'} = $params[1];
		$filterOpts->{'date'}      = $date;

		my @ids = db::get_material_ids_a( $dbh, $filterOpts );
		my $count = @ids;
		my $ids = db::arr_to_csv(@ids);
					
		if ( $count > 0 )	# ensure materials exist on this branch
		{    

			my @tokens = split( /-/, $date );
			my $yy     = $tokens[0];
			my $mm     = $tokens[1];
			my $dd     = $tokens[2];

			# convert to unix ts
			my $ts = timelocal( "00", "00", "00", $dd, $mm - 1, $yy ) * 1000;

				if ( param("noDataAndCountIn") ) {		# do not add a count of the materials on this branch
					
					push(
						@t_data,
						{
							name       => $date,
							uiProvider => 'Ext.tree.ColumnNodeUI',
							id => $level . "," . $params[1] . "," . $date,
							preloadChildren => 'true'
						}
					);
				}
				else {
					push(
						@t_data,
						{
							name       => $date,
							uiProvider => 'Ext.tree.ColumnNodeUI',
							id => $level . "," . $params[1] . "," . $date,
							preloadChildren => 'true',
							materialCount   => $count,
							materialIds     => $ids
						}
					);
				}
		}
	}
}

#
# 3: timestamp level
#
elsif ( $level == 3 ) {
	my @timecodes = db::get_unique_times( $dbh, $params[1], $params[2] );

	foreach my $tc (@timecodes) {
		$filterOpts->{'projectid'} = $params[1];
		$filterOpts->{'date'}      = $params[2];
		$filterOpts->{'timecode'}  = $tc;
		
		my $tcStr = db::get_timecode_string($tc);

		my @ids = db::get_material_ids_a( $dbh, $filterOpts );
		my $count = @ids;
		my $ids = db::arr_to_csv(@ids);
		
		if ( $count > 0 )	 # ensure materials exist on this branch
		{   
			if ( param("noDataAndCountIn") ) {		# do not add a count of the materials on this branch
				
				push(
					@t_data,
					{
						name       => $tcStr,
						uiProvider => 'Ext.tree.ColumnNodeUI',
						id         => $level . ","
						  . $params[1] . ","
						  . $params[2] . ","
						  . $tc,
						preloadChildren => 'true'
					}
				);
			}

			else {
				
				push(
					@t_data,
					{
						name       => $tcStr,
						uiProvider => 'Ext.tree.ColumnNodeUI',
						id         => $level . ","
						  . $params[1] . ","
						  . $params[2] . ","
						  . $tc,
						preloadChildren => 'true',
						materialCount   => $count,
						materialIds     => $ids
					}
				);
			}

		}
	}
}

#
# 4: materials level
#
elsif ( $level == 4 ) {
	$filterOpts->{'video'}    = $videoFormat;
	$filterOpts->{'keywords'} = $keywords;

	my @materials = db::load_materials( $dbh, $params[1], $params[2], $params[3] );
	my @tree = db::prepare_exttree_json( @materials, $filterOpts );

	@t_data = @tree;

}

my $json = encode_json [@t_data];
print "Content-Type: application/json; charset=ISO-8859-1\n\n";
print $json;


# 
# convert set time period to start and end times
#
sub rangeToTimestamp {
	my ( $range, $day ) = @_;

	my $out;

	my $fromDate = time;
	my $toDate   = $fromDate;
	my $fromTime = 0;
	my $toTime   = 0;

	if ( $day == 1 )    # today
	{
	}
	elsif ( $day == 2 )    # yesterday
	{
		$fromDate -= 24 * 60 * 60;
		$toDate   -= 24 * 60 * 60;
	}
	elsif ( $day == 3 )    # 2 days ago
	{
		$fromDate -= 2 * 24 * 60 * 60;
		$toDate   -= 2 * 24 * 60 * 60;
	}
	else                   # 3 days ago
	{
		$fromDate -= 3 * 24 * 60 * 60;
		$toDate   -= 3 * 24 * 60 * 60;
	}

	if ( $range == 1 )     # last 10 minutes
	{
		my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($fromDate);

		$toTime = $sec + $min * 60 + $hour * 60 * 60;

		$min -= 10;
		if ( $min < 0 ) {
			$min = 60 + $min;
			$hour -= 1;
			if ( $hour < 0 ) {

				# we don't go back beyond the start of day
				$hour = 0;
				$min  = 0;
				$sec  = 0;
			}
		}
		$fromTime = $sec + $min * 60 + $hour * 60 * 60;
	}
	elsif ( $range == 2 )    # last 20 minutes
	{
		my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($fromDate);

		$toTime = $sec + $min * 60 + $hour * 60 * 60;

		$min -= 20;
		if ( $min < 0 ) {
			$min = 60 + $min;
			$hour -= 1;
			if ( $hour < 0 ) {

				# we don't go back beyond the start of day
				$hour = 0;
				$min  = 0;
				$sec  = 0;
			}
		}
		$fromTime = $sec + $min * 60 + $hour * 60 * 60;
	}
	elsif ( $range == 3 )    # morning
	{
		$fromTime = 0;
		$toTime   = 12 * 60 * 60;    # 12 hours
	}
	elsif ( $range == 4 )            # afternoon
	{
		$fromTime = 12 * 60 * 60;    # 12 hours
		$toTime   = 0;
		$toDate += 24 * 60 * 60;
	}
	elsif ( $range == 5 )            # day
	{
		$fromTime = 0;
		$toTime   = 0;
		$toDate += 24 * 60 * 60;
	}
	elsif ( $range == 6 )            # 2 days
	{
		$fromTime = 0;
		$toTime   = 0;
		$toDate += 2 * 24 * 60 * 60;
	}
	else                             # 3 days
	{
		$fromTime = 0;
		$toTime   = 0;
		$toDate += 3 * 24 * 60 * 60;
	}

	my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($fromDate);
	$fromDate = timelocal_nocheck( 0, 0, 0, $mday, $mon, $year + 1900 );

	$fromDate += $fromTime;

	( $sec, $min, $hour, $mday, $mon, $year ) = localtime($toDate);
	$toDate = timelocal_nocheck( 0, 0, 0, $mday, $mon, $year + 1900 );
	$toDate += $toTime;

	$out->{'startTimestamp'} = $fromDate * 1000;
	$out->{'endTimestamp'}   = $toDate * 1000;

	return $out;
}


#
# error loading data
#
sub loader_error {
	my ($errorStr) = @_;
	die $errorStr;
}
