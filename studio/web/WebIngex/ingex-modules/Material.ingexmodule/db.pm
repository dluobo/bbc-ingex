# Copyright (C) 2008  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
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

use strict;
use warnings;
use lib ".";
use lib "../../ingex-config";
use ingexhtmlutil;
use prodautodb;

use DBI qw(:sql_types);

####################################
#
# Module exports
#
####################################

BEGIN 
{
    use Exporter ();
    our (@ISA, @EXPORT, @EXPORT_OK);

    @ISA = qw(Exporter);
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
    my ($dbh, $pkgId) = @_;

    my $localError = "unknown error";
    my $package;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pkg_identifier AS id,
                pkg_uid AS uid,
                pkg_name AS name,
                pkg_creation_date AS creation_date,
                pkg_project_name_id AS project_name_id,
                pjn_name AS project_name,
                pkg_descriptor_id AS descriptor_id
            FROM Package
                LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier)
            WHERE
                pkg_identifier = ?
            ");
        $sth->bind_param(1, $pkgId, SQL_INTEGER);
        $sth->execute;

        if ($package = $sth->fetchrow_hashref())
        {
            $sth->finish();
            
            # load the descriptor if it is a source package
            if ($package->{"DESCRIPTOR_ID"})
            {
                $sth = $dbh->prepare("
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
                    ");
                $sth->bind_param(1, $package->{"DESCRIPTOR_ID"}, SQL_INTEGER);
                $sth->execute;
        
                if (!($package->{"descriptor"} = $sth->fetchrow_hashref()))
                {
                    $localError = "Failed to load descriptor with id $package->{'DESCRIPTOR_ID'} "
                        . "for package with id $pkgId";
                    die;
                }

                $sth->finish();
            }
            
            # load the tracks
            my $sth = $dbh->prepare("
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
                ");
            $sth->bind_param(1, $pkgId, SQL_INTEGER);
            $sth->execute;
    
            if ($package->{"tracks"} = $sth->fetchall_arrayref({}))
            {
                $sth->finish();
                
                # load the source clip for each track
                foreach my $track (@{ $package->{"tracks"} })
                {
                    $sth = $dbh->prepare("
                        SELECT scp_identifier AS id,
                            scp_source_package_uid AS source_package_uid,
                            scp_source_track_id AS source_track_id,
                            scp_length AS length,
                            scp_position AS position
                        FROM SourceClip
                        WHERE
                            scp_track_id = ?
                        ");
                    $sth->bind_param(1, $track->{"ID"}, SQL_INTEGER);
                    $sth->execute;

                    if (!($track->{"sourceclip"} = $sth->fetchrow_hashref()))
                    {
                        $localError = "Failed to load source clip for " 
                            . "track with id $track->{'ID'} "
                            . "for package with id $pkgId";
                        die;
                    }
                    
                    $sth->finish();
                }
            }
            else
            {
                $localError = "Failed to load package track for package with id $pkgId";
                die;
            }
            
            # load user comments
            $sth = $dbh->prepare("
                SELECT uct_identifier AS id,
                    uct_name AS name,
                    uct_value AS value
                FROM UserComment
                WHERE
                    uct_package_id = ?
                ");
            $sth->bind_param(1, $pkgId, SQL_INTEGER);
            $sth->execute;
    
            $package->{"usercomments"} = $sth->fetchall_arrayref({});

            $sth->finish();
        }
        else
        {
            $localError = "Failed to load package with id $pkgId";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $package;
}

sub load_referenced_package
{
    my ($dbh, $sourcePackageUID, $sourceTrackID) = @_;

    my $localError = "unknown error";
    my $package;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pkg_identifier AS id 
            FROM Package
            WHERE
                pkg_uid = ?
            ");
        $sth->bind_param(1, $sourcePackageUID, SQL_VARCHAR);
        $sth->execute;

        my $packageRef;
        if ($packageRef = $sth->fetchrow_hashref())
        {
            $package = load_package($dbh, $packageRef->{"ID"})
                or $localError = "Failed to load package with id = $packageRef->{'ID'}: $prodautodb::errstr" and die;

            # check that the track with track_id = sourceTrackID is in there            
            my $foundTrack = 0;
            foreach my $track (@ { $package->{"tracks"} })
            {
                if ($track->{"TRACK_ID"} == $sourceTrackID)
                {
                    $foundTrack = 1;
                    last;
                }
            }
            if (!$foundTrack)
            {
                $localError = "referenced package with uid $sourcePackageUID does not "
                    . "have track with id $sourceTrackID";
                die;
            }
        }
        else
        {
            $localError = "Failed to load referenced package with uid '$sourcePackageUID'";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $package;
}

sub load_package_chain
{
    my ($dbh, $package, $refPackages, $refChainLen, $maxRefChainLen) = @_;

    if (defined $refChainLen && defined $maxRefChainLen && $refChainLen >= $maxRefChainLen)
    {
        return 1;
    }
    
    my $localError = "unknown error";
    eval
    {
        foreach my $track (@{ $package->{"tracks"} })
        {
            my $sourceClip = $track->{"sourceclip"};
            
            if ($sourceClip->{"SOURCE_PACKAGE_UID"} !~ /\\0*/) # reference chain is not terminated
            {
                if ($sourceClip->{"SOURCE_PACKAGE_UID"} eq $package->{"UID"})
                {
                    $localError = "Package with id $package->{'ID'} references itself";
                    die;
                }
                if (! $refPackages->{ $sourceClip->{"SOURCE_PACKAGE_UID"} })
                {
                    my $refPackage = load_referenced_package($dbh, 
                        $sourceClip->{"SOURCE_PACKAGE_UID"}, $sourceClip->{"SOURCE_TRACK_ID"})
                        or next;

                    $refPackages->{ $refPackage->{"UID"} } = $refPackage;
                        
                    load_package_chain($dbh, $refPackage, $refPackages, 
                        $refChainLen ? $refChainLen + 1 : 1, $maxRefChainLen)
                        or $localError = "Failed to load package chain for package "
                            . "with id = $package->{'ID'}: $prodautodb::errstr" and die;
                }
            }
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return 0;
    }

    return 1;    
}

sub load_material_1
{
    my ($dbh, $offset, $count) = @_;

    my %material;
    
    my $localError = "unknown error";
    eval
    {
        # load the ids of the material packages
        my $sth = $dbh->prepare("
            SELECT 
                pkg_identifier AS id 
            FROM Package
            WHERE
                pkg_descriptor_id IS NULL
            ORDER BY pkg_creation_date DESC
            OFFSET $offset
            LIMIT $count
            ");
        $sth->execute;

        while (my $pkg = $sth->fetchrow_hashref())
        {
            my $pkgId = $pkg->{"ID"};
            
            # load the whole material package
            my $materialPackage = load_package($dbh, $pkgId)
                or $localError = "Failed to load material package with id = $pkgId: $prodautodb::errstr" and die;
            $material{"materials"}->{ $materialPackage->{"UID"} } = $materialPackage;
            $material{"packages"}->{ $materialPackage->{"UID"} } = $materialPackage;

            # load the package chain referenced by the material package
            # load only the referenced file package and referenced tape/live source package
            load_package_chain($dbh, $materialPackage, $material{"packages"}, 0, 2)
                or $localError = "Failed to load package chain for material package with id=$pkgId: $prodautodb::errstr" and die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%material;
}

sub get_material_count
{
    my ($dbh) = @_;

    my $count;
    
    my $localError = "unknown error";
    eval
    {
        my $sth = $dbh->prepare("
            SELECT count(pkg_identifier) AS count
            FROM Package
            WHERE
                pkg_descriptor_id IS NULL
            ");
        $sth->execute;

        if (my $row = $sth->fetchrow_hashref())
        {
            $count = $row->{"COUNT"};
        }
        else
        {
            $localError = "Failed to get material package count";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return -1;
    }
    
    return $count || 0;
}


1;

