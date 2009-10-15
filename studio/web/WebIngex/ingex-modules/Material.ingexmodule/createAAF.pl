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
# createAAF script - executes create_aaf executable using supplied json parameters
#
##########################################################################################

use strict;
use warnings;

use lib "../../ingex-modules/Material.ingexmodule";
use lib "../../ingex-config";

use ingexconfig;
use materialconfig;
use db;
use ingexhtmlutil;

use XML::Simple;
use File::Temp qw(mktemp tempfile);
use JSON::XS;
use CGI qw(:standard);

# unix tmp directory
my $tempdir = '/tmp';

# get parameters from json
my $jsonStr = param('jsonIn');
my $json = decode_json($jsonStr);

# set correct export path from json parameters
my $fnPrefix = $json->{"Root"}->[0]->{"ApplicationSettings"}->[0]->{"FnamePrefix"};
my $dir = $json->{"Root"}->[0]->{"ApplicationSettings"}->[0]->{"ExportDir"};
my $prefix = get_path($dir, $fnPrefix);
$json->{"Root"}->[0]->{"ApplicationSettings"}->[0]->{"Prefix"} = $prefix;

# set database parameters from ingex config
$json->{"Root"}->[0]->{"RunSettings"}->[0]->{"DBHostName"}  = $ingexConfig{"db_host"};
$json->{"Root"}->[0]->{"RunSettings"}->[0]->{"DBName"}  = $ingexConfig{"db_name"};
$json->{"Root"}->[0]->{"RunSettings"}->[0]->{"User"} = $ingexConfig{"db_user"};
$json->{"Root"}->[0]->{"RunSettings"}->[0]->{"Password"} = $ingexConfig{"db_password"};

# re-encode json as xml config
my $xml = XMLout( $json, RootName => undef );
my ( $fh, $filename ) = tempfile( DIR => $tempdir );	# create unique temp file
print $fh $xml;		# write xml to temp file

print header;

# run create_aaf using xml config
eval{	
	$json = create_aaf($filename);
};
if($@){
	print "err~$@";	# error
}
else{
	print "ok~$json";	# success
}

unlink($filename);	# clean up


#
# create a path from directory / filename
#
sub get_path {
	my ( $dir, $fnprefix ) = @_;

	my $exportDir = get_avid_aaf_export_dir($dir);
	my ($filenamePrefix) = $fnprefix =~ /^\s*(.*\S)\s*$/;
	$filenamePrefix =~ s/\"/\\\"/g;

	my $prefix;
	if ( !$exportDir || $exportDir =~ /\/$/ ) {
		$prefix = join( "", $exportDir, $filenamePrefix );
	}
	else {
		$prefix = join( "/", $exportDir, $filenamePrefix );
	}

	return $prefix;
}


#
# Execute create_aaf using supplied configuration xml
# Returns json encoded results
#
sub create_aaf {
	my ($xmlFile) = @_;
	$xmlFile =~ s/\"/\\\"/g; # untaint

	my $errFilename = mktemp("/tmp/createaafresultsXXXXXX");
	my $resultsFilename = mktemp("/tmp/createaafresultsXXXXXX");
	
	my $cmd = join (
		 				" ", 
		 				"create_aaf", 
		 				"--xml-command", 
		 				$xmlFile, 
		 				"2>$errFilename",		# pipe results of STDERR to file
		 				"1>$resultsFilename"	# pipe results of STDOUT to file
	 				);

	# required path parameters
	$ENV{"PATH"} = $ingexConfig{"create_aaf_dir"};
	$ENV{"LD_LIBRARY_PATH"} = $ingexConfig{"create_aaf_dir"};

	# execute command
	my $out = system($cmd);

	# an error occurred
	if ($out != 0) {
		
		# extract the message from STDERR
		open( AAFERROR, "<", "$errFilename" )
			or aaf_error(
				"Failed to open AAF error file: $!", $cmd, $xml
			);
		my $err_msg;
		
		while ( my $line = <AAFERROR> ) {
			chomp($line);
			$err_msg .= $line."\n";
		}
		
		aaf_error(
			"Failed to export Edit file. Using create_aaf in location: $ingexConfig{'create_aaf_dir'} \n"."<p style=\"color: red\">Error: $err_msg</p>", $cmd, $xml
	  	);
	}
	else{
		# extract the results
		open( AAFRESULTS, "<", "$resultsFilename" )
			or aaf_error(
				"Failed to open AAF results file: $!", $cmd, $xml
			);
	
		my $index = 0;
		my $totalClips;
		my $totalMulticamGroups;
		my $totalDirectorsCutSequences;
		my @filenames;
		my $haveResultsHeader;
		my %results;
	
		$results{"filename"} = $xmlFile;
	
		while ( my $line = <AAFRESULTS> ) {
			chomp($line);
	
			if ($haveResultsHeader) {
				if ( $index == 0 ) {
					$totalClips = $line;
					$results{"totalclips"} = $totalClips;
				}
				elsif ( $index == 1 ) {
					$totalMulticamGroups = $line;
					$results{"totalmultigroups"} = $totalMulticamGroups;
				}
				elsif ( $index == 2 ) {
					$totalDirectorsCutSequences = $line;
					$results{"totaldircut"} = $totalDirectorsCutSequences;
				}
				else {
					push( @filenames, { 'file' => $line } );
				}
	
				$index++;
			}
			elsif ( $line =~ /^RESULTS/ ) {
				$haveResultsHeader = 1;
			}
		}
	
		$results{"filenames"} = [@filenames];
	
		close(AAFRESULTS);
		unlink($resultsFilename);	# clean up
	
		if ( !scalar @filenames ) {
			# No materials returned
		}
	
		$json = encode_json( \%results );
		return $json;
	}
}


#
# error creating aaf
#
sub aaf_error {
	my ($errorStr, $cmd, $xml) = @_;
	
	# store in logfile
	log_message("ERROR: Error creating AAF! ".$errorStr);
	log_message("Command: ".$cmd);
	log_message("XML input: \n".$xml);
	
	die $errorStr;
}
