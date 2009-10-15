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
#


##########################################################################################
#
# ajaxcall script - Handles ajax/json requests from javascript for various functions such 
# as deleting materials and changing default settings.
#
##########################################################################################

use strict;
use warnings;

use lib "../../ingex-config";
use lib "../../ingex-modules/Material.ingexmodule";

use materialconfig;
use db;
use fileActions;

use JSON::XS;
use CGI qw(:standard);


my $dbh = prodautodb::connect(
			$ingexConfig{"db_host"},
	        $ingexConfig{"db_name"},
	        $ingexConfig{"db_user"},
	        $ingexConfig{"db_password"}
        ) 
    or die();

my $response;
my $result;
my $errstr = '';

# get parameters from json
my $jsonStr = param('jsonIn');
my $json = decode_json($jsonStr);
my $call_type = $json->{"typeIn"}; 

print header;

if($call_type eq "get_aff_config"){
	$response = getAAFConfig();
}

elsif($call_type eq "delete"){
	$response = deleteMaterials();
}

print "$response";


#
# permanently delete material packages from database
#
sub deleteMaterials
{
	my $packages = $json->{"packages"};
	my @toDelete;
	my $package;
	
	# iterate through package items
	for (my $i=0; $i<@$packages; $i++){	
	
		# each package item can either be an individual material package id, or a set of parameters describing multiple packages
		
		# individual material id
		if($packages->[$i]{materialId})
		{
			my $id = $packages->[$i]{materialId};
			push(@toDelete, $id);
		}
			
		# multiple packages
		else{
			# filter opts
			my $videoFormat = $packages->[$i]{"formatIn"};
			my $startTime = $packages->[$i]{"tStartIn"};
			my $endTime = $packages->[$i]{"tEndIn"};
			my $tcStr = $packages->[$i]{"timeIn"};
			my $date = $packages->[$i]{"dateIn"};
			my $projectId = $packages->[$i]{"projectIn"};
			my $keywords = $packages->[$i]{"keywordsIn"};	# key search terms
			
			my $tcVal = db::get_timecode_val($tcStr);
	
			my $filterOpts;
			$filterOpts->{'video'} = $videoFormat; 
			$filterOpts->{'keywords'} = $keywords;
			$filterOpts->{'startTime'} = $startTime;
			$filterOpts->{'endTime'} = $endTime;
			$filterOpts->{'timecode'} = $tcVal;
			$filterOpts->{'date'} = $date;
			$filterOpts->{'projectid'} = $projectId;
			
			my @ids = db::get_material_ids_a($dbh, $filterOpts);
			push(@toDelete, @ids);
		}
	}

	#delete them
	eval{
		$result = delete_packages(\@toDelete);
	};
	if($@){
		return "err~$@";	# error
	}
	
	return "ok~".$result;
}


#
# get default export options for materials
#
sub getAAFConfig
{
	my $fileFormat = $ingexConfig{"format"};
	my $exportPath = $ingexConfig{"export_path"};
	my $avidAAFPrefix = $ingexConfig{"avid_aaf_prefix"};
	my $longSuffix = $ingexConfig{"long_suffix"};
	my $editPath = $ingexConfig{"edit_path"};
	my $directorsCut = $ingexConfig{"directors_cut"};
	my $directorsSource = $ingexConfig{"directors_cut_source"};
	my $audioEdit = $ingexConfig{"audio_edit"};
	
	my %results = 	(
						'FCP' => $fileFormat,
						'ExportDir' => $exportPath,
						'FnamePrefix' => $avidAAFPrefix,
						'LongSuffix' => $longSuffix,
						'EditPath' => $editPath,
						'DirCut' => $directorsCut,
						'DirSource' => $directorsSource,
						'AudioEdit' => $audioEdit
					);
	
	$json = encode_json(\%results);
	return "ok~".$json;
}