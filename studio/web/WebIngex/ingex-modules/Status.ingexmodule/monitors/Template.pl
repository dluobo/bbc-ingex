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
use Switch;
use lib ".";
use lib "..";
use IngexStatus;

# Specify some basics about your monitor...
my $friendlyName = "Template Monitor";
# poll time in ms or false for no poll (or for a leech - see below)
my $pollTime = 200;
# The data fields your monitor produces, and their types
my $fields = {"output"=>"simpleText"};
# The types of node which your monitor should be applied to
my $appliesTo = ["Recorder","FileStore"];

# Specify any thresholds used by your rendering javascript - note, this hash must not be empty, simply omit it if not needed
my $thresholds = {"over_seventy_percent"=>70,"under_fifty_percent"=>-50};

# Where to place this monitor in the sort order on the overview page (0 = don't display in overview)
my $sortOrder = 200;

# Specify the type, and if applicable, any type-specific settings
# A "standard" monitor executes this file to get data with a command line argument of basicInfo (or availability)
# An "http" monitor is queried via http, and requires a queryLoc
# A "leech" monitor doesn't get queried, but instead uses the data from another monitor. Updates are triggered when the monitor it
#	leeches from us updated, therefore a leech monitor's pollTime is meaningless. The name of the monitor to leech from should be
#	specified in the queryLoc
my $queryType = "standard";

my $queryLoc = ":7000"; # defines port for a direct http-queried monitor or a host monitor for a leech.
# n.b. you can define a query location such as "/state" or ":7000/state" 
# HOWEVER any action other than basicInfo (specifically "availability") MUST be handled at url.queryLoc/action
# e.g. myhostname:7000/availability
# so you would have to handle myhostname/state/availability

# Define how to handle various options...
switch ($ARGV[0]){
	#format: case "eventToHandle" { functionToRun(); }
	case "monitorInfo" { monitorInfo(); }
	case "basicInfo" { basicInfo(); }
	case "availbility" { availability(); }
}

exit(1);

#Your handler functions...

# Each should generally print JSON-formatted data out.
# It's generally easiest to do this by creating a hash and using hashToJSON()

sub monitorInfo
{
	my %monitorInfo;
	$monitorInfo{"friendlyName"}=$friendlyName;
	$monitorInfo{"fields"}=$fields;
	$monitorInfo{"pollTime"}=$pollTime;
	$monitorInfo{"appliesTo"}=$appliesTo;
	$monitorInfo{"queryType"}=$queryType;
	$monitorInfo{"queryLoc"}=$queryLoc;
	$monitorInfo{"sortOrder"}=$sortOrder;
	# remember to remove this line if no thresholds specified...
	$monitorInfo{"thresholds"}=$thresholds;
	print hashToJSON(%$monitorInfo);
}

sub basicInfo
{
	# create a hash for the output, with a reference to another containing monitor data
	my %basicInfo;
	my %data;
	$basicInfo{"monitorData"} = \%data;
	
	# define the state, and populate the fields within monitorData
	$basicInfo{"state"} = "ok";
	$data{"output"} = "Simple test monitor!";
	
	# output the results
	print hashToJSON(%basicInfo);
}

sub availability
{
	my %data;
	
	# define the state, and populate the fields within monitorData
	$data{"availability"} = "true";
	
	# output the results
	print hashToJSON(%data);
}