# Copyright (C) 2009  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
# Modified by: Sean Casey <seantc@users.sourceforge.net>
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

package db;

use warnings;

use lib ".";
use lib "../../ingex-config";

use Time::Local;
use Time::localtime;

use ingexhtmlutil;
use prodautodb;
use ingexconfig;
use materialconfig;

use DBI qw(:sql_types);
use JSON::XS;

####################################
#
# Module exports
#
####################################

BEGIN 
{
	use Exporter ();
	our ( @ISA, @EXPORT, @EXPORT_OK );

	@ISA    = qw(Exporter);
	@EXPORT = qw(
	  &load_package
	  &load_referenced_package
	  &load_package_chain
	  &load_material_1
	  &get_material_count
	);

}

####################################
#
# Package
#
####################################

sub load_package 
{
	my ( $dbh, $pkgId ) = @_;

	my $localError = "unknown error";
	my $package;
	eval {
		my $sth =
		  $dbh->prepare( "
	            SELECT pkg_identifier AS id,
	                pkg_uid AS uid,
	                pkg_name AS name,
	                pkg_creation_date AS creation_date,
	                pkg_project_name_id AS project_name_id,
	                pjn_name AS project_name,
	                pkg_descriptor_id AS descriptor_id,
	                pkg_op_id AS op_id
	            FROM Package
	                LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier)
	            WHERE
	                pkg_identifier = ?
            " );
		$sth->bind_param( 1, $pkgId, {TYPE => SQL_INTEGER} );
		$sth->execute;

		if ( $package = $sth->fetchrow_hashref() ) {
			$sth->finish();

			# load the descriptor if it is a source package
			if ( $package->{"DESCRIPTOR_ID"} ) {
				$sth = $dbh->prepare( "
	                    SELECT eds_identifier AS id,
	                        eds_essence_desc_type AS type,
	                        eds_file_location AS file_location,
	                        eds_file_format AS file_format_id,
							fft_name AS file_format,
	                        eds_video_resolution_id AS video_res_id,
	                        vrn_name AS video_res,
	                        (eds_image_aspect_ratio).numerator AS aspect_ratio_num,
	                        (eds_image_aspect_ratio).denominator AS aspect_ratio_den,
	                        eds_audio_quantization_bits AS audio_quant_bits,
	                        eds_spool_number AS spool_number,
	                        eds_recording_location AS recording_location_id
	                    FROM EssenceDescriptor
	                        LEFT OUTER JOIN VideoResolution ON (eds_video_resolution_id = vrn_identifier)
							LEFT OUTER JOIN FileFormat ON (eds_file_format = fft_identifier)
	                    WHERE
	                        eds_identifier = ?
                    " );
				$sth->bind_param( 1, $package->{"DESCRIPTOR_ID"}, {TYPE => SQL_INTEGER} );
				$sth->execute;

				if ( !( $package->{"descriptor"} = $sth->fetchrow_hashref() ) )
				{
					$localError = 	"Failed to load descriptor with id $package->{'DESCRIPTOR_ID'}"
					  			   ."for package with id $pkgId";
					die;
				}

				$sth->finish();
			}

			# load the tracks
			my $sth = $dbh->prepare( "
		                SELECT trk_identifier AS id,
		                    trk_id AS track_id,
		                    trk_number AS track_number,
		                    trk_name AS name,
		                    trk_data_def AS data_def_id,
		                    (trk_edit_rate).numerator AS edit_rate_num,
		                    (trk_edit_rate).denominator AS edit_rate_den
		                FROM Track
		                WHERE
		                    trk_package_id = ?
                	" );
			$sth->bind_param( 1, $pkgId, {TYPE => SQL_INTEGER} );
			$sth->execute;

			if ( $package->{"tracks"} = $sth->fetchall_arrayref( {} ) ) {
				$sth->finish();

				# load the source clip for each track
				foreach my $track ( @{ $package->{"tracks"} } ) {
					$sth = $dbh->prepare( "
	                        SELECT scp_identifier AS id,
	                            scp_source_package_uid AS source_package_uid,
	                            scp_source_track_id AS source_track_id,
	                            scp_length AS length,
	                            scp_position AS position
	                        FROM SourceClip
	                        WHERE
	                            scp_track_id = ?
                        " );
					$sth->bind_param( 1, $track->{"ID"}, {TYPE => SQL_INTEGER} );
					$sth->execute;

					if (
						!( $track->{"sourceclip"} = $sth->fetchrow_hashref() ) )
					{
						$localError =
							    "Failed to load source clip for "
							  . "track with id $track->{'ID'} "
							  . "for package with id $pkgId";
						die;
					}

					$sth->finish();
				}
			}
			else {
				$localError = "Failed to load package track for package with id $pkgId";
				die;
			}

			# load user comments
			$sth = $dbh->prepare( "
                SELECT uct_identifier AS id,
                    uct_name AS name,
                    uct_value AS value
                FROM UserComment
                WHERE
                    uct_package_id = ?
                " );
			$sth->bind_param( 1, $pkgId, {TYPE => SQL_INTEGER} );
			$sth->execute;

			$package->{"usercomments"} = $sth->fetchall_arrayref( {} );

			$sth->finish();
		}
		else {
			$localError = "Failed to load package with id $pkgId";
			die;
		}
	};
	if ($@) {
		$prodautodb::errstr = ( defined $dbh->errstr ) ? $dbh->errstr : $localError;
		return undef;
	}

	return $package;
}

sub load_referenced_package 
{
	my ( $dbh, $sourcePackageUID, $sourceTrackID ) = @_;

	my $localError = "unknown error";
	my $package;
	eval {
		my $sth =
		  $dbh->prepare( "
            SELECT pkg_identifier AS id 
            FROM Package
            WHERE
                pkg_uid = ?
            " );
		$sth->bind_param( 1, $sourcePackageUID, {TYPE => SQL_VARCHAR} );
		$sth->execute;

		my $packageRef;
		if ( $packageRef = $sth->fetchrow_hashref() ) {
			$package = load_package( $dbh, $packageRef->{"ID"} )
			  or $localError = "Failed to load package with id = $packageRef->{'ID'}: $prodautodb::errstr"
			  	and die;

			# check that the track with track_id = sourceTrackID is in there
			my $foundTrack = 0;
			foreach my $track ( @{ $package->{"tracks"} } ) {
				if ( $track->{"TRACK_ID"} == $sourceTrackID ) {
					$foundTrack = 1;
					last;
				}
			}
			if ( !$foundTrack ) {
				$localError =
				    "referenced package with uid $sourcePackageUID does not "
				  . "have track with id $sourceTrackID";
				die;
			}
		}
		else {
			$localError =
			  "Failed to load referenced package with uid '$sourcePackageUID'";
			die;
		}
	};
	if ($@) {
		$prodautodb::errstr = ( defined $dbh->errstr ) ? $dbh->errstr : $localError;
		return undef;
	}

	return $package;
}

sub load_package_chain 
{
	my ( $dbh, $package, $refPackages, $refChainLen, $maxRefChainLen ) = @_;

	if (   defined $refChainLen
		&& defined $maxRefChainLen
		&& $refChainLen >= $maxRefChainLen )
	{
		return 1;
	}

	my $localError = "unknown error";
	eval {
		foreach my $track ( @{ $package->{"tracks"} } )
		{
			my $sourceClip = $track->{"sourceclip"};

			if ( $sourceClip->{"SOURCE_PACKAGE_UID"} !~
				/\\0*/ )    # reference chain is not terminated
			{
				if ( $sourceClip->{"SOURCE_PACKAGE_UID"} eq $package->{"UID"} )
				{
					$localError =
					  "Package with id $package->{'ID'} references itself";
					die;
				}
				if ( !$refPackages->{ $sourceClip->{"SOURCE_PACKAGE_UID"} } ) {
					my $refPackage = load_referenced_package(
						$dbh,
						$sourceClip->{"SOURCE_PACKAGE_UID"},
						$sourceClip->{"SOURCE_TRACK_ID"}
					  )
					  or next;

					$refPackages->{ $refPackage->{"UID"} } = $refPackage;

					load_package_chain( $dbh, $refPackage, $refPackages,
						$refChainLen ? $refChainLen + 1 : 1,
						$maxRefChainLen )
					  or $localError =
					    "Failed to load package chain for package "
					  . "with id = $package->{'ID'}: $prodautodb::errstr"
					  and die;
				}
			}
		}
	};
	if ($@) {
		$prodautodb::errstr = ( defined $dbh->errstr ) ? $dbh->errstr : $localError;
		return 0;
	}

	return 1;
}


#####################################################################
# loading subroutines for loading tree data on the fly
#

#
# load project names
#
sub load_projects {
	my ( $dbh, $projectid ) = @_;

	eval {
		my @cond;
		if($projectid) { push (@cond, "WHERE pjn_identifier = ?"); }
		my $cond = join(" ", @cond);
		
		my $query = "
	            SELECT pjn_identifier AS id, 
	                pjn_name AS name
	            FROM Projectname
	            	$cond
	            ORDER BY pjn_name
	            ";
		my $sth = $dbh->prepare( $query );

		if( $projectid ) {$sth->bind_param( 1, $projectid, {TYPE => SQL_INTEGER} )};

		$sth->execute;

		$proj = $sth->fetchall_arrayref( {} );

	};
	if ($@) {
		db_error("Failed to load project names: $dbh->errstr");
	}

	return $proj;
}


#
# load unique times for a specific project and date
#
sub get_unique_times 
{
	my ( $dbh, $projectid, $date ) = @_;
	my $timecodes;

	eval {
		$timecodes = load_times( $dbh, $projectid, $date );
	};
	if ($@) {
		db_error($@);
	}
			  	
	# find unique times and store
	my @u_times;
	my $pos;
	my $time;

	foreach my $timecode ( @{$timecodes} ) {
		push( @u_times, $timecode->{'TC'} );
	}

	return @u_times;
}


#
# load all start timecodes for specified project name and date
#
sub load_times 
{
	my ( $dbh, $projectid, $date ) = @_;
	my $timecodes;

	eval {
		my $query = "
				select DISTINCT file_position AS TC
					from
					(select * 
						from
						(select * 
							from
							(select file_position ,
								pkg_uid as file_pkg_uid
								from
								(select file_position,
									trk_package_id as file_trk_package_id
									from
									(select scp_position as file_position,
										scp_track_id as file_track_id
										from
											sourceclip
									) as file_scp
									left join track on file_scp.file_track_id=track.trk_identifier
										where trk_id=1
								) as file_trk
								left join package on file_trk.file_trk_package_id=package.pkg_identifier
							) as file_package
							left join sourceclip on file_package.file_pkg_uid=sourceclip.scp_source_package_uid
						) as scp
						left join track on scp.scp_track_id=track.trk_identifier
					) as trk
				left join package on trk.trk_package_id=package.pkg_identifier

				WHERE pkg_project_name_id = ?
					AND CAST(pkg_creation_date AS TEXT) ~ ?
					AND pkg_descriptor_id IS NULL
				";
		my $sth = $dbh->prepare( $query );
		$sth->bind_param( 1, $projectid, {TYPE => SQL_INTEGER} );
		$sth->bind_param( 2, "^".$date );
		$sth->execute;
		$timecodes = $sth->fetchall_arrayref( {} );
	};
	if ($@) {
		db_error("Failed to load times for project $projectid and date $date: $dbh->errstr");
	}

	return $timecodes;
}


#
# count the number of materials items that match supplied search criteria
#
sub count_materials 
{
	my ( $dbh, $opts ) = @_;
	my $count;
	
	eval {
		$count = get_materials( $dbh, $opts, 1 );
	};
	if ($@) {
		db_error("Error counting materials: $@");
	}
	return $count;
}


#
# convert array to series of csv
#
sub arr_to_csv
{
	my (@arr) = @_;
	my $csv = join( ",", @arr );
	return $csv;
}


#
# get csv string of ids
#
sub get_material_ids 
{
	my ( $dbh, $opts ) = @_;
	my @ids;
	
	eval {
		@ids = get_materials( $dbh, $opts, 2 );
	};
	if ($@) {
		db_error($@);
	}
	my $idstr = join( ",", @ids );
	return $idstr;
}


#
# get array of ids
#
sub get_material_ids_a 
{
	my ( $dbh, $opts ) = @_;
	my @ids;
	
	eval {
		 @ids = get_materials( $dbh, $opts, 2 );	
	};
	if ($@) {
		db_error($@);
	}
	return @ids;
}


#
# return either an array of material ids or a numeric count of all material packages that match the supplied parameters
#
sub get_materials 
{
	my ( $dbh, $opts, $mode ) = @_;
	my $count = 0;
	my @material_ids;

	eval{
		# mode = 1 - count
		# mode = 2 - get ids
		
		# add necessary 'where' conditions
		my @cond;
		if($opts->{'timecode'}) { push (@cond, "AND file_scp.scp_position = ?"); }
		if($opts->{'date'}) { push (@cond, "AND CAST(material_pkg.pkg_creation_date AS TEXT) ~ ?"); }
		if($opts->{'video'} ne "-1") { push (@cond, "AND descriptor.eds_video_resolution_id = ?"); }
		if($opts->{'projectid'} && $opts->{'projectid'} >= 0) { push (@cond, "AND material_pkg.pkg_project_name_id = ?"); }
		if($opts->{'keywords'}) { push (@cond, "AND CAST(material_pkg.pkg_name AS TEXT) ~* ?"); }
		my $cond = join("\n", @cond);
		
		
		$query = "
				select 
					DISTINCT ON(material_pkg.pkg_identifier)
					
					material_pkg.pkg_identifier as PKG_IDENTIFIER,
					file_scp.scp_position as file_position,
					(file_trk.trk_edit_rate).numerator AS edit_rate_num,
					(file_trk.trk_edit_rate).denominator AS edit_rate_den,
					material_pkg.pkg_creation_date as pkg_creation_date,
					file_trk.trk_number as trk_number
				
					from 
				
						sourceclip AS 		file_scp,
					 	track AS 		file_trk,
						package AS 		file_pkg,
				
						essencedescriptor AS 	descriptor,
				
						sourceclip AS	 	material_scp,
					 	track AS 		material_trk,
						package AS 		material_pkg
				
				
					where 
						file_trk.trk_identifier = file_scp.scp_track_id AND
						file_pkg.pkg_identifier = file_trk.trk_package_id AND
				
						descriptor.eds_identifier = file_pkg.pkg_descriptor_id AND
				
						material_scp.scp_source_package_uid = file_pkg.pkg_uid AND
						material_trk.trk_identifier = material_scp.scp_track_id AND
						material_pkg.pkg_identifier = material_trk.trk_package_id AND
				
						file_trk.trk_id = 1
						AND material_pkg.pkg_descriptor_id IS NULL
						$cond
					";
						
	
		$sth = $dbh->prepare($query);
		
		# bind parameters
		my $i = 1;	
		if ($opts->{'timecode'}) 		{ $sth->bind_param( $i, $opts->{'timecode'}, {TYPE => SQL_DOUBLE} ); $i++; }  		# timecode
		if ($opts->{'date'}) 			{ $sth->bind_param( $i, "^".$opts->{'date'}, {TYPE => SQL_VARCHAR} ); $i++; }		# date
		if ($opts->{'video'} ne "-1") 	{ $sth->bind_param( $i, $opts->{'video'}, {TYPE => SQL_INTEGER} ); $i++; }    		# video resolution
		if ($opts->{'projectid'} && $opts->{'projectid'} >= 0) 	
										{ $sth->bind_param( $i, $opts->{'projectid'}, {TYPE => SQL_INTEGER} ); $i++; }    	# project id
		if($opts->{'keywords'}) 		{ $sth->bind_param( $i, $opts->{'keywords'}, {TYPE => SQL_VARCHAR} ); $i++; }		# search terms

		$sth->execute;
	
		# now filter these results by start and end time
		
		# start/end time codes have been specified
		if ( $opts->{'startTimestamp'} && $opts->{'endTimestamp'} ) {
			while ( $row = $sth->fetchrow_hashref() ) {
				# start time code
				$startTC = 	$row->{"FILE_POSITION"} * 
							(25 / $row->{"EDIT_RATE_NUM"}) *
							$row->{"EDIT_RATE_DEN"};
				my $hms = timecode_to_hms($startTC);
				my $hh  = $hms->{'hh'};
				my $min = $hms->{'mm'};
				my $ss  = $hms->{'ss'};  
				my $ms  = $hms->{'ms'};
				
				# start creation date
				my $datetime  = $row->{'PKG_CREATION_DATE'};
				my $id        = $row->{'PKG_IDENTIFIER'};
				my @tokens    = split( / /, $datetime );
				my $date = $tokens[0];
				
				@tokens = split( /-/, $date );
				my $yy = $tokens[0];
				my $mm = $tokens[1];
				my $dd = $tokens[2];
				
				# convert to unix timestamp
				my $sourceTimestamp = (timelocal( $ss, $min, $hh, $dd, $mm - 1, $yy ) * 1000) + $ms;
				
				if($sourceTimestamp >= $opts->{'startTimestamp'} && $sourceTimestamp <= $opts->{'endTimestamp'}){
					# date and timecode of material is within boundaries
					$count++;
					push( @material_ids, $id );
				}
			}
		}
	
		# no start/end time supplied
		else {
			$count = $sth->rows;
			while ( $row = $sth->fetchrow_hashref() ) {
				my $id = $row->{'PKG_IDENTIFIER'};
				push( @material_ids, $id );
			}
		}
	};
	if ($@) {
		db_error("Error retrieving materials: ".$@);
	}
	
	if    ( $mode == '1' ) { return $count; }
	elsif ( $mode == '2' ) { return @material_ids; }
}


#
# convert timecode value to hour, min, sec
#
sub timecode_to_hms{
	my ($timecode) = @_;
	my $hms;
	
	# TODO: Do not fix at 25fps
	$hms->{'hh'} = int($timecode / (60 * 60 * 25));
    $hms->{'mm'} = int(($timecode % (60 * 60 * 25)) / (60 * 25));
    $hms->{'ss'} = int((($timecode % (60 * 60 * 25)) % (60 * 25)) / 25);
    $fr = int(((($timecode % (60 * 60 * 25)) % (60 * 25)) % 25));	#frame number 0-24
    $hms->{'ff'} = $fr;
	$hms->{'ms'} = 1000 * ($fr / 25);
	
	return $hms;
}


#
# load unique dates for a specific project name
#
sub get_unique_dates 
{
	my ( $dbh, $projectid ) = @_;
	my $date_times;
	
	eval{
		$date_times = load_dates( $dbh, $projectid );
	};
	if ($@) {
		db_error($@);
	}

	# find unique dates and store
	my @u_dates;
	my $pos;
	my $date;

	my %hash = ();

	foreach my $date_time ( @{$date_times} ) {
		$pos = index( $date_time->{'DATE'}, " " );
		$date =
		  substr( $date_time->{'DATE'}, 0, $pos );   # strip time part from date/time pair

		@match = grep { $_ eq $date } @u_dates;  	 # search for next date in list

		if ( !@match ) {
			push( @u_dates, $date );
		}
	}

	return @u_dates;
}


#
# load dates
#
sub load_dates 
{
	my ( $dbh, $projectid ) = @_;
	my $dates;

	eval {
		my $sth =
		  $dbh->prepare( "
				SELECT DISTINCT pkg_creation_date AS date
				FROM package
				WHERE pkg_project_name_id = ?
			" );
		$sth->bind_param( 1, $projectid, {TYPE => SQL_INTEGER} );
		$sth->execute;
		$dates = $sth->fetchall_arrayref( {} );
	};
	if ($@) {
		db_error("Error loading dates for project $projectid: $dbh->errstr");
	}

	return $dates;
}


sub load_materials 
{
	my ( $dbh, $projectid, $date, $timecode ) = @_;

	my %material;

	my $localError = "unknown error";
	eval {
			$query = "select 
				DISTINCT ON(material_pkg.pkg_identifier)
				
				material_pkg.pkg_identifier as ID
			
				from 
			
					sourceclip AS 		file_scp,
				 	track AS 		file_trk,
					package AS 		file_pkg,
			
					sourceclip AS	 	material_scp,
				 	track AS 		material_trk,
					package AS 		material_pkg
			
			
				where 
					file_trk.trk_identifier = file_scp.scp_track_id AND
					file_pkg.pkg_identifier = file_trk.trk_package_id AND
			
					material_scp.scp_source_package_uid = file_pkg.pkg_uid AND
					material_trk.trk_identifier = material_scp.scp_track_id AND
					material_pkg.pkg_identifier = material_trk.trk_package_id AND
			
					file_scp.scp_position = ? AND
					CAST (material_pkg.pkg_creation_date AS TEXT) ~ ? AND	
					material_pkg.pkg_descriptor_id IS NULL AND
					material_pkg.pkg_project_name_id = ?
			";
		
		my $sth = $dbh->prepare( $query );
		
        $sth->bind_param( 1, $timecode, {TYPE => SQL_INTEGER} );
        $sth->bind_param( 2, "^".$date, {TYPE => SQL_VARCHAR} );
		$sth->bind_param( 3, $projectid, {TYPE => SQL_INTEGER} );
		
		$sth->execute;

		while ( my $pkg = $sth->fetchrow_hashref() ) {
			my $pkgId = $pkg->{"ID"};

			# load the whole material package
			my $materialPackage = load_package( $dbh, $pkgId )
			 	or $localError = "Failed to load material package with id = $pkgId: $prodautodb::errstr"
			  	and die;
			$material{"materials"}->{ $materialPackage->{"UID"} } = $materialPackage;
			$material{"packages"}->{ $materialPackage->{"UID"} } = $materialPackage;

			 # load the package chain referenced by the material package
			 # load only the referenced file package and referenced tape/live source package
			load_package_chain( $dbh, $materialPackage, $material{"packages"}, 0, 2 )
				or $localError = "Failed to load package chain for material package with id=$pkgId: $prodautodb::errstr"
			 	and die;
		}
	};
	if ($@) {
		db_error("Error loading materials for project $projectid, date $date and timecode $timecode: ".$@);
	}

	return \%material;
}


# 
# Prepare json in array format suitable for javascript ext grid
#
sub prepare_exttree_json 
{
	my ( $material, $filterOpts ) = @_;

	my $json;
	my @material_array;
	my %next;
	my %aa;

	my $branchCount = 0;
	my $nodeCount   = 0;
	my @childNodes;

	# adapted from old materials page...
	if ( defined $material ) {
		my @materialRows;
		my $index = 0;

		foreach my $mp ( values %{ $material->{"materials"} } ) {
			my %materialRow;
			my $startTC;
			my $endTC;
			my $duration;

			$materialRow{"name"}    = $mp->{"NAME"}          || "";
			$materialRow{"created"} = $mp->{"CREATION_DATE"} || "";

			my @videoTrackNumberRanges;
			my @audioTrackNumberRanges;
			foreach my $track ( @{ $mp->{"tracks"} } ) {
				if ( $track->{"DATA_DEF_ID"} == 1 ) {
					merge_track_numbers( \@videoTrackNumberRanges,
						$track->{'TRACK_NUMBER'} );
				}
				else {
					merge_track_numbers( \@audioTrackNumberRanges,
						$track->{'TRACK_NUMBER'} );
				}

				my $sourceClip = $track->{"sourceclip"};

				#TODO: don't hard code edit rate at 25 fps??
				if ( !defined $duration ) {
					$duration =
					  $sourceClip->{"LENGTH"} *
					  ( 25 / $track->{"EDIT_RATE_NUM"} ) *
					  $track->{"EDIT_RATE_DEN"};
					$materialRow{"duration"} = get_duration_string($duration);
				}

				if ( $sourceClip->{"SOURCE_PACKAGE_UID"} !~ /\\0*/ ) {
					my $sourcePackage =
					  $material->{"packages"}->{ $sourceClip->{"SOURCE_PACKAGE_UID"} };

					if ($sourcePackage 
						&& $sourcePackage->{"descriptor"} 
						&& $sourcePackage->{"descriptor"}->{"TYPE"} == 1
					  )    # is file source package
					{

						#added
						if ( !defined $materialRow{"id"} ) {
							$materialRow{"id"} = $mp->{"ID"};
						}
						## -------------------

						my $descriptor = $sourcePackage->{"descriptor"};

						if ( !defined $materialRow{"video"} ) {

							# the video track has the video format
							if ($descriptor && $descriptor->{"VIDEO_RES"}) {
								$materialRow{"video"} =
								    $descriptor->{"FILE_FORMAT"} . ": "
								  . $descriptor->{"VIDEO_RES"};
								$materialRow{"vresid"} =
								  $descriptor->{"VIDEO_RES_ID"};
							}
						}

						# added - file urls for each material
						my $type;
						if ( $track->{"DATA_DEF_ID"} == 1 ) {
							$type = 'V';
						}
						else {
							$type = 'A';
						}
						my $trackName = $type . $track->{'TRACK_NUMBER'};
						$materialRow{"file_location"}{$trackName} = $ingexConfig{"video_path"}."/".$descriptor->{"FILE_LOCATION"};

						if (scalar @{ $sourcePackage->{"tracks"} } >= 1 )    # expecting 1 track in the file package
						{
							my $sourceTrack = $sourcePackage->{"tracks"}[0];
							$sourceClip = $sourceTrack->{"sourceclip"};

							if ( !defined $startTC ) {

								#TODO: don't hard code edit rate at 25 fps??
								$startTC =
								  $sourceClip->{"POSITION"} *
								  ( 25 / $sourceTrack->{"EDIT_RATE_NUM"} ) *
								  $sourceTrack->{"EDIT_RATE_DEN"};
								$materialRow{"startTC"} = get_timecode_string($startTC);
							}

							$sourcePackage =
							  $material->{"packages"}->{ $sourceClip->{"SOURCE_PACKAGE_UID"} };

							if ($sourcePackage && $sourcePackage->{"descriptor"}    	# is source package
									&& ($sourcePackage->{"descriptor"}->{"TYPE"} ==	2 	# is tape source package
									|| $sourcePackage->{"descriptor"}->{"TYPE"} == 3	# is live source package
								))                      
							{
								$materialRow{"tapeOrLive"} =
								  $sourcePackage->{"NAME"};
							}
						}
					}
				}
			}
			my $first = 1;
			$materialRow{"tracks"} = "";
			foreach my $range (@videoTrackNumberRanges) {
				if ($first) {
					$materialRow{"tracks"} .= "V";
					$first = 0;
				}
				else {
					$materialRow{"tracks"} .= ",";
				}
				if ( $range->[0] != $range->[1] ) {
					$materialRow{"tracks"} .= "$range->[0]-$range->[1]";
				}
				else {
					$materialRow{"tracks"} .= "$range->[0]";
				}
			}
			$first = 1;
			foreach my $range (@audioTrackNumberRanges) {
				if ($first) {
					$materialRow{"tracks"} .= "&nbsp;"
					  if $materialRow{"tracks"};
					$materialRow{"tracks"} .= "A";
					$first = 0;
				}
				else {
					$materialRow{"tracks"} .= ",";
				}
				if ( $range->[0] != $range->[1] ) {
					$materialRow{"tracks"} .= "$range->[0]-$range->[1]";
				}
				else {
					$materialRow{"tracks"} .= "$range->[0]";
				}
			}

			$endTC = $startTC + $duration
			  if defined $startTC && defined $duration;
			if ($endTC) {
				$materialRow{"endTC"} = get_timecode_string($endTC);
			}

			if ( $mp->{"usercomments"} ) {
				foreach my $userComment ( @{ $mp->{"usercomments"} } ) {
					if ( !defined $materialRow{"descript"} ) {
						$materialRow{"descript"} = $userComment->{"VALUE"}
						  if ( $userComment->{"NAME"} eq "Descript" );
					}
					if ( !defined $materialRow{"comments"} ) {
						$materialRow{"comments"} = $userComment->{"VALUE"}
						  if ( $userComment->{"NAME"} eq "Comments" );
					}
					last
					  if $materialRow{"descript"} && $materialRow{"comments"};
				}
			}

			push( @materialRows, \%materialRow );

			$index++;
		}


		foreach my $materialRow ( sort materialCmp @materialRows ) {

			# only add if this leaf matches selected filter options
			if ( filterMatch( $materialRow, $filterOpts ) ) {

				# new leaf
				my $fileLocs = $materialRow->{"file_location"};

				$childNodes[$nodeCount] = (
					{
						name        => $materialRow->{"name"},
						created     => $materialRow->{"created"},
						start       => $materialRow->{"startTC"} || "",
						end         => $materialRow->{"endTC"} || "",
						duration    => $materialRow->{"duration"} || "",
						video       => $materialRow->{"video"} || "",
						tapeid      => $materialRow->{"tapeOrLive"} || "",
						url         => { %{ $materialRow->{"file_location"} } },
						id          => $materialRow->{"id"} || "",
						comments    => $materialRow->{"comments"} || "",
						description => $materialRow->{"descript"} || "",
						vresid 		=> $materialRow->{"vresid"} || "",

						uiProvider => 'Ext.tree.ColumnNodeUI',
						leaf       => 'true',
						cls        => 'styles.css',
						iconCls    => 'video'
					}
				);
				$nodeCount++;
			}
		}
	}


	return @childNodes;
}


#
# compares material item to selected filter options and returns true if it satisfies these, false otherwise
#
sub filterMatch 
{
	my ( $material, $filterOpts ) = @_;

	if ( $filterOpts->{"video"} > -1 ) {    # -1 = any video res
		if ( $material->{"vresid"} != $filterOpts->{"video"} ) {

			# invalid
			return 0;
		}
	}

	my $match = 0;
	my @keywords = split( / /, $filterOpts->{"keywords"} );
	if ( @keywords > 0 ) {                  # if array size > 0

		foreach my $word (@keywords) {

			if ( $material->{"name"} =~ m/$word/i ) {   # case insensitive match
				$match = 1;
			}
		}

		if ( !$match ) {

			# no key words found
			return 0;
		}
	}

	# material meets filter
	return 1;
}


sub merge_track_numbers 
{
	my ( $numberRanges, $number ) = @_;

	my $index    = 0;
	my $inserted = 0;
	foreach my $range ( @{$numberRanges} ) {
		if ($inserted) {
			if ( $number == $range->[0] - 1 ) {

				# merge range with previous range
				$numberRanges->[ $index - 1 ]->[1] = $range->[1];
				delete $numberRanges->[$index];
			}
			last;
		}
		else {
			if ( $number == $range->[0] || $number == $range->[0] - 1 ) {

				# extend range to left
				$range->[0] = $number;
				$inserted = 1;
			}
			elsif ( $number == $range->[1] || $number == $range->[1] + 1 ) {

				# extend range to right
				$range->[1] = $number;
				$inserted = 1;
			}
		}

		$index++;
	}

	if ( !$inserted ) {
		push( @{$numberRanges}, [ $number, $number ] );
	}
}


#
# convert timecode string to a value understood by database
#
sub get_timecode_val
{
	my ($tcStr) = @_;
	
	my @vals = split (/:/, $tcStr);
	
	$hour = $vals[0];
	$min = $vals[1];
	$sec = $vals[2];
	$fr = $vals[3];
	
	my $tc = 	($hour * (60 * 60 * 25)) +
				($min * (60 * 25)) +
				($sec * 25) +
				($fr);
				
	return $tc;
}


sub get_timecode_string 
{
	my ($position) = @_;

	my ( $hour, $min, $sec, $fr );

	$hour = int( $position / ( 60 * 60 * 25 ) );
	$min = int( ( $position % ( 60 * 60 * 25 ) ) / ( 60 * 25 ) );
	$sec = int( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) / 25 );
	$fr = int( ( ( ( $position % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) % 25 ) );

	return sprintf( "%02d:%02d:%02d:%02d", $hour, $min, $sec, $fr );
}


sub get_duration_string 
{
	my ($duration) = @_;

	my ( $hour, $min, $sec, $fr );

	$hour = int( $duration / ( 60 * 60 * 25 ) );
	$min = int( ( $duration % ( 60 * 60 * 25 ) ) / ( 60 * 25 ) );
	$sec = int( ( ( $duration % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) / 25 );
	$fr = int( ( ( ( $duration % ( 60 * 60 * 25 ) ) % ( 60 * 25 ) ) % 25 ) );

	my $durationString;
	if ( $hour > 0 ) {
		$durationString =
		  sprintf( "%02d:%02d:%02d:%02d", $hour, $min, $sec, $fr );
	}
	elsif ( $min > 0 ) {
		$durationString = sprintf( "%02d:%02d:%02d", $min, $sec, $fr );
	}
	else {
		$durationString = sprintf( "%02d:%02d", $sec, $fr );
	}

	return $durationString;
}


sub materialCmp 
{
	if ( $a->{"created"} eq $b->{"created"} ) {
		return $a->{"name"} cmp $b->{"name"};
	}
	else {
		return $b->{"created"} cmp $a->{"created"};
	}
}


#
# ingex db error
#
sub db_error{
	my ($errorStr) = @_;
	die $errorStr;
}

1;
