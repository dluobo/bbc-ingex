#
# $Id: prodautodb.pm,v 1.4 2009/10/12 18:38:23 john_f Exp $
#
# 
#
# Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
 
package prodautodb;

use strict;
use warnings;

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
        &dump_table
        &load_recording_locations
        &save_recording_location 
        &update_recording_location 
        &delete_recording_location
        &load_video_resolutions
        &load_video_resolution
        &load_source_configs
        &load_source_config
        &save_source_config
        &update_source_config
        &delete_source_config
        &load_source_config_refs
        &load_recorders
        &load_recorder
        &save_recorder
        &update_recorder
        &delete_recorder
        &load_default_recorder_parameters
        &load_recorder_configs
        &load_recorder_config
        &save_recorder_config
        &update_recorder_config
        &delete_recorder_config
        &load_multicam_configs
        &load_multicam_config
        &save_multicam_config
        &update_multicam_config
        &delete_multicam_config
        &load_package
        &load_referenced_package
        &load_package_chain
        &load_material_1
        &get_material_count
        &load_proxy_types
        &load_proxy_defs
        &load_proxy_def
        &save_proxy_def
        &update_proxy_def
        &delete_proxy_def
        &load_material_for_proxy
        &delete_material_for_proxy
        &have_proxy
        &save_proxy
        &load_video_resolutions
        &load_router_configs
        &load_router_config
        &save_router_config
        &update_router_config
        &delete_router_config
    );

    @EXPORT_OK = qw(
        &connect 
        &disconnect 
        %sourceType 
        %dataDef 
        $errstr
    );
}


####################################
#
# Exported package globals
#
####################################

our $errstr = "";

our %sourceType = (
    "Tape"              => 1,
    "LiveRecording"     => 2
);

our %dataDef = (
    "Picture"           => 1,
    "Sound"             => 2
);



####################################
#
# Connect
#
####################################


sub connect 
{
    my ($host, $database, $user, $password) = @_;
    
    my $dsn = "DBI:Pg:dbname=$database;host=$host";
    
    my $dbh;
    eval
    {
        $dbh = DBI->connect($dsn, $user, $password) || die $DBI::errstr;
    };
    if ($@)
    {
        $errstr = $DBI::errstr;
        return undef;
    }
    
    $dbh->{RaiseError} = 1;
    $dbh->{AutoCommit} = 0;
    $dbh->{FetchHashKeyName} = "NAME_uc"; 
    
    return $dbh;
}

sub prodautodb::disconnect 
{
    my ($dbh) = @_;
    
    eval { $dbh->disconnect || warn $dbh->errstr; };
}



####################################
#
# Dump
#
####################################

sub dump_table
{
    my ($host, $database, $user, $password, $dump, $name) = @_;

    # untaint PATH and restore original value later
    my $oldPath = $ENV{PATH};
    $ENV{PATH} = "";
    
    my $IN;
    if (! open(IN, "/usr/local/pgsql/bin/pg_dump -U $user -d $database -h $host --table $name --data-only |") )
    {
        $ENV{PATH} = $oldPath; 
        return 0;
    }
    push(@{ $dump }, <IN>);
    
    $ENV{PATH} = $oldPath;
    return 1;
}



####################################
#
# Table sequences
#
####################################

# note: callers of this function must handle exceptions
sub _load_next_id
{
    my ($dbh, $seq) = @_;

    my $nextId;
    my $sth = $dbh->prepare("
        SELECT nextval(?)
        ");
    $sth->bind_param(1, $seq);
    $sth->execute;
    
    $nextId = $sth->fetchrow();
    
    return $nextId;
}


####################################
#
# Recording Locations
#
####################################

sub load_recording_locations
{
    my ($dbh) = @_;
    
    my $rls;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT rlc_identifier AS id, 
                rlc_name AS name
            FROM RecordingLocation
            ");
        $sth->execute;
        
        $rls = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $rls;    
}

sub load_recording_location
{
    my ($dbh, $rlcId) = @_;
    
    my $localError = "unknown error";
    my $rlc;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT rlc_identifier AS id, 
                rlc_name AS name
            FROM RecordingLocation
            WHERE
                rlc_identifier = ?
            ");
        $sth->bind_param(1, $rlcId, SQL_INTEGER);
        $sth->execute;
        
        unless ($rlc = $sth->fetchrow_hashref())
        {
            $localError = "Failed to load recording location with id $rlcId";
            die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $rlc;    
}

sub save_recording_location
{
    my ($dbh, $name) = @_;

    my $nextId;
    eval
    {
        $nextId = _load_next_id($dbh, "rlc_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO RecordingLocation 
                (rlc_identifier, rlc_name)
            VALUES
                (?, ?)
            ");
        $sth->bind_param(1, $nextId, SQL_INTEGER);
        $sth->bind_param(2, $name, SQL_VARCHAR);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = $dbh->errstr;
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_recording_location
{
    my ($dbh, $rlc) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE RecordingLocation
            SET
                rlc_name = ?
            WHERE
                rlc_identifier = ?
            ");
        $sth->bind_param(1, $rlc->{'NAME'}, SQL_VARCHAR);
        $sth->bind_param(2, $rlc->{'ID'}, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = $dbh->errstr;
        eval { $dbh->rollback; };
        return 0;
    }

    return 1;    
}

sub delete_recording_location
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM RecordingLocation
            WHERE
                rlc_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

####################################
#
# Video resolutions
#
####################################

sub load_video_resolutions
{
    my ($dbh) = @_;
    
    my $vrs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT vrn_identifier AS id, 
                vrn_name AS name
            FROM VideoResolution
            ");
        $sth->execute;
        
        $vrs = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $vrs;    
}

sub load_video_resolution
{
    my ($dbh, $vrnId) = @_;
    
    my $localError = "unknown error";
    my $vrn;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT vrn_identifier AS id, 
                vrn_name AS name
            FROM VideoResolution
            WHERE
                vrn_identifier = ?
            ");
        $sth->bind_param(1, $vrnId, SQL_INTEGER);
        $sth->execute;
        
        unless ($vrn = $sth->fetchrow_hashref())
        {
            $localError = "Failed to load video resolution with id $vrnId";
            die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $vrn;    
}



####################################
#
# Source configuration
#
####################################


sub load_source_configs
{
    my ($dbh) = @_;

    my @scfs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT scf_identifier AS id, 
                scf_name AS name,
                scf_type AS type_id,
                srt_name AS type,
                scf_spool_number AS spool,
                scf_recording_location AS location_id,
                rlc_name AS location
            FROM SourceConfig
                LEFT OUTER JOIN RecordingLocation ON (scf_recording_location = rlc_identifier)
                INNER JOIN SourceConfigType ON (scf_type = srt_identifier)
            ORDER BY scf_name
            ");
        $sth->execute;

        while (my $scf = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT sct_identifier AS id, 
                    sct_track_id AS track_id,
                    sct_track_number AS track_number,
                    sct_track_name AS name,
                    sct_track_data_def AS data_def_id,
                    ddf_name AS data_def,
                    (sct_track_edit_rate).numerator AS edit_rate_num,
                    (sct_track_edit_rate).denominator AS edit_rate_den,
                    sct_track_length AS length
                FROM SourceTrackConfig
                    INNER JOIN SourceConfig ON (sct_source_id = scf_identifier)
                    INNER JOIN DataDefinition ON (sct_track_data_def = ddf_identifier)
                WHERE
                    scf_identifier = ?
                ORDER BY sct_track_id
                ");
            $sth2->bind_param(1, $scf->{"ID"});
            $sth2->execute;
            
            push(@scfs, {
                config => $scf,
                tracks => $sth2->fetchall_arrayref({})
                });
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@scfs;
}

sub load_source_config
{
    my ($dbh, $scfId) = @_;

    my $localError = "unknown error";
    my %scf;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT scf_identifier AS id, 
                scf_name AS name,
                scf_type AS type_id,
                srt_name AS type,
                scf_spool_number AS spool,
                scf_recording_location AS location_id,
                rlc_name AS location
            FROM SourceConfig
                LEFT OUTER JOIN RecordingLocation ON (scf_recording_location = rlc_identifier)
                INNER JOIN SourceConfigType ON (scf_type = srt_identifier)
            WHERE
                scf_identifier = ?
            ");
        $sth->bind_param(1, $scfId);
        $sth->execute;

        if ($scf{'config'} = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT sct_identifier AS id, 
                    sct_track_id AS track_id,
                    sct_track_number AS track_number,
                    sct_track_name AS name,
                    sct_track_data_def AS data_def_id,
                    ddf_name AS data_def,
                    (sct_track_edit_rate).numerator AS edit_rate_num,
                    (sct_track_edit_rate).denominator AS edit_rate_den,
                    sct_track_length AS length
                FROM SourceTrackConfig
                    INNER JOIN SourceConfig ON (sct_source_id = scf_identifier)
                    INNER JOIN DataDefinition ON (sct_track_data_def = ddf_identifier)
                WHERE
                    scf_identifier = ?
                ORDER BY sct_track_id
                ");
            $sth2->bind_param(1, $scfId);
            $sth2->execute;
            
            $scf{'tracks'} = $sth2->fetchall_arrayref({});
        }
        else
        {
            $localError = "Failed to load source config with id $scfId";
            die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }

    return \%scf;
}

sub save_source_config
{
    my ($dbh, $scf) = @_;

    my $nextSourceId;
    eval
    {
        $nextSourceId = _load_next_id($dbh, "scf_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO SourceConfig 
                (scf_identifier, scf_name, scf_type, scf_spool_number, scf_recording_location)
            VALUES
                (?, ?, ?, ?, ?)
            ");
        $sth->bind_param(1, $nextSourceId, SQL_INTEGER);
        $sth->bind_param(2, $scf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $scf->{"config"}->{"TYPE_ID"}, SQL_INTEGER);
        $sth->bind_param(4, $scf->{"config"}->{"SPOOL"}, SQL_VARCHAR);
        $sth->bind_param(5, $scf->{"config"}->{"LOCATION_ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $scf->{"tracks"} })
        {
            my $nextTrackId = _load_next_id($dbh, "sct_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO SourceTrackConfig 
                    (sct_identifier, sct_track_id, sct_track_number, sct_track_name, sct_track_data_def,
                    sct_track_edit_rate, sct_track_length, sct_source_id)
                VALUES
                (?, ?, ?, ?, ?, (?, ?), ?, ?)
                ");
            $sth->bind_param(1, $nextTrackId, SQL_INTEGER);
            $sth->bind_param(2, $track->{"TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(3, $track->{"TRACK_NUMBER"}, SQL_INTEGER);
            $sth->bind_param(4, $track->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(5, $track->{"DATA_DEF_ID"}, SQL_INTEGER);
            $sth->bind_param(6, $track->{"EDIT_RATE_NUM"}, SQL_INTEGER);
            $sth->bind_param(7, $track->{"EDIT_RATE_DEN"}, SQL_INTEGER);
            $sth->bind_param(8, $track->{"LENGTH"}, SQL_INTEGER);
            $sth->bind_param(9, $nextSourceId, SQL_INTEGER);
            $sth->execute;
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextSourceId;
}

sub update_source_config
{
    my ($dbh, $scf) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE SourceConfig
            SET
                scf_name = ?, 
                scf_type = ?, 
                scf_spool_number = ?, 
                scf_recording_location = ?
            WHERE
                scf_identifier = ?
            ");
        $sth->bind_param(1, $scf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $scf->{"config"}->{"TYPE_ID"}, SQL_INTEGER);
        $sth->bind_param(3, $scf->{"config"}->{"SPOOL"}, SQL_VARCHAR);
        $sth->bind_param(4, $scf->{"config"}->{"LOCATION_ID"}, SQL_INTEGER);
        $sth->bind_param(5, $scf->{"config"}->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $scf->{"tracks"} })
        {
            my $nextTrackId = _load_next_id($dbh, "sct_id_seq");
        
            my $sth = $dbh->prepare("
                UPDATE SourceTrackConfig
                SET
                    sct_track_id = ?, 
                    sct_track_number = ?, 
                    sct_track_name = ?, 
                    sct_track_data_def = ?,
                    sct_track_edit_rate = (?, ?), 
                    sct_track_length = ?, 
                    sct_source_id = ?
                WHERE
                    sct_identifier = ?
                ");
            $sth->bind_param(1, $track->{"TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(2, $track->{"TRACK_NUMBER"}, SQL_INTEGER);
            $sth->bind_param(3, $track->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(4, $track->{"DATA_DEF_ID"}, SQL_INTEGER);
            $sth->bind_param(5, $track->{"EDIT_RATE_NUM"}, SQL_INTEGER);
            $sth->bind_param(6, $track->{"EDIT_RATE_DEN"}, SQL_INTEGER);
            $sth->bind_param(7, $track->{"LENGTH"}, SQL_INTEGER);
            $sth->bind_param(8, $scf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(9, $track->{"ID"}, SQL_INTEGER);
            $sth->execute;
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_source_config
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM SourceConfig
            WHERE
                scf_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub load_source_config_refs
{
    my ($dbh) = @_;

    my $scfrs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT scf_identifier AS id, 
                scf_name AS name,
                sct_track_id AS track_id,
                sct_track_name AS track_name
            FROM SourceConfig
                INNER JOIN SourceTrackConfig ON (sct_source_id = scf_identifier)
            ORDER BY scf_name, sct_track_id
            ");
        $sth->execute;

        $scfrs = $sth->fetchall_arrayref({});
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $scfrs;
}


####################################
#
# Recorder
#
####################################

sub load_recorders
{
    my ($dbh) = @_;

    my $recs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT rer_identifier AS id, 
                rer_name AS name,
                rer_conf_id AS conf_id,
                rec_name AS conf_name
            FROM Recorder
                LEFT OUTER JOIN RecorderConfig ON (rer_conf_id = rec_identifier)
            ORDER BY rer_name
            ");
        $sth->execute;

        $recs = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $recs;
}

sub load_recorder
{
    my ($dbh, $recId) = @_;

    my $localError = "unknown error";
    my $rec;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT rer_identifier AS id, 
                rer_name AS name,
                rer_conf_id AS conf_id,
                rec_name AS conf_name
            FROM Recorder
                LEFT OUTER JOIN RecorderConfig ON (rer_conf_id = rec_identifier)
            WHERE
                rer_identifier = ?
            ORDER BY rer_name
            ");
        $sth->bind_param(1, $recId);
        $sth->execute;

        if (!($rec = $sth->fetchrow_hashref()))
        {
            $localError = "Failed to load recorder with id $recId";
            die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $rec;
}

sub save_recorder
{
    my ($dbh, $rec) = @_;

    my $nextRecorderId;
    eval
    {
        $nextRecorderId = _load_next_id($dbh, "rer_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Recorder 
                (rer_identifier, rer_name, rer_conf_id)
            VALUES
                (?, ?, ?)
            ");
        $sth->bind_param(1, $nextRecorderId, SQL_INTEGER);
        $sth->bind_param(2, $rec->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $rec->{"RECORDER_CONF_ID"}, SQL_INTEGER);
        $sth->execute;
        
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextRecorderId;
}

sub update_recorder
{
    my ($dbh, $rec) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE Recorder
            SET
                rer_name = ?, 
                rer_conf_id = ? 
            WHERE
                rer_identifier = ?
            ");
        $sth->bind_param(1, $rec->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $rec->{"CONF_ID"}, SQL_INTEGER);
        $sth->bind_param(3, $rec->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_recorder
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM Recorder
            WHERE
                rer_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}




####################################
#
# Recorder configuration
#
####################################

sub load_default_recorder_parameters
{
    my ($dbh) = @_;
    
    my $drps;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT drp_identifier AS id, 
                drp_name AS name,
                drp_value AS value,
                drp_type AS type
            FROM DefaultRecorderParameter
            ");
        $sth->execute;
        
        $drps = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $drps;    
}

sub load_recorder_configs
{
    my ($dbh, $recId) = @_; # recId is optional

    my @rcfs;
    eval
    {
        my $sth;
        
        if (defined $recId)
        {
            # configs associated with a specific recorder
            $sth = $dbh->prepare("
                SELECT rec_identifier AS id, 
                    rec_name AS name,
                    rec_recorder_id AS recorder_id,
                    rer_name AS recorder_name
                FROM RecorderConfig
                    INNER JOIN Recorder ON (rec_recorder_id = rer_identifier)
                WHERE
                    rec_recorder_id = ?
                ORDER BY rec_name
                ");
            $sth->bind_param(1, $recId);
        }
        else
        {
            # all configs
            $sth = $dbh->prepare("
                SELECT rec_identifier AS id, 
                    rec_name AS name,
                    rec_recorder_id AS recorder_id,
                    rer_name AS recorder_name
                FROM RecorderConfig
                    INNER JOIN Recorder ON (rec_recorder_id = rer_identifier)
                ORDER BY rec_name
                ");
        }
        $sth->execute;

        while (my $rcf = $sth->fetchrow_hashref())
        {
            # load recorder parameters
            
            my $sth2 = $dbh->prepare("
                SELECT rep_identifier AS id, 
                    rep_name AS name,
                    rep_value AS value,
                    rep_type AS type
                FROM RecorderParameter
                    INNER JOIN RecorderConfig ON (rep_recorder_conf_id = rec_identifier)
                WHERE
                    rep_recorder_conf_id = ?
                ORDER BY rep_name
                ");
            $sth2->bind_param(1, $rcf->{"ID"});
            $sth2->execute;

            my $rps = $sth2->fetchall_arrayref({});

            # load recorder input configs
            
            my $sth3 = $dbh->prepare("
                SELECT ric_identifier AS id, 
                    ric_index AS index,
                    ric_name AS name
                FROM RecorderInputConfig
                    INNER JOIN RecorderConfig ON (ric_recorder_conf_id = rec_identifier)
                WHERE
                    ric_recorder_conf_id = ?
                ORDER BY ric_index
                ");
            $sth3->bind_param(1, $rcf->{"ID"});
            $sth3->execute;

            my @ricfs;            
            while (my $ricf = $sth3->fetchrow_hashref())
            {
                # load recorder input track configs
            
                my $sth4 = $dbh->prepare("
                    SELECT rtc_identifier AS id, 
                        rtc_index AS index,
                        rtc_track_number AS track_number,
                        scf_name AS source_name,
                        rtc_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM RecorderInputTrackConfig
                        INNER JOIN RecorderInputConfig ON (rtc_recorder_input_id = ric_identifier)
                        LEFT OUTER JOIN SourceConfig ON (rtc_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (rtc_source_id = sct_source_id AND
                            rtc_source_track_id = sct_track_id)
                    WHERE
                        rtc_recorder_input_id = ?
                    ORDER BY rtc_index
                    ");
                $sth4->bind_param(1, $ricf->{"ID"});
                $sth4->execute;
                
                push(@ricfs, {
                    config => $ricf,
                    tracks => $sth4->fetchall_arrayref({})
                });
            }
            
            push(@rcfs, {
                config => $rcf,
                parameters => $rps,
                inputs => \@ricfs
                });
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@rcfs;
}

sub load_recorder_config
{
    my ($dbh, $rcfId) = @_;

    my $localError = "unknown error";
    my %rcf;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT rec_identifier AS id, 
                rec_name AS name,
                rec_recorder_id AS recorder_id,
                rer_name AS recorder_name
            FROM RecorderConfig
                INNER JOIN Recorder ON (rec_recorder_id = rer_identifier)
            WHERE
                rec_identifier = ?
            ");
        $sth->bind_param(1, $rcfId);
        $sth->execute;

        if ($rcf{'config'} = $sth->fetchrow_hashref())
        {
            # load recorder parameters
            
            my $sth2 = $dbh->prepare("
                SELECT rep_identifier AS id, 
                    rep_name AS name,
                    rep_value AS value,
                    rep_type AS type
                FROM RecorderParameter
                    INNER JOIN RecorderConfig ON (rep_recorder_conf_id = rec_identifier)
                WHERE
                    rep_recorder_conf_id = ?
                ORDER BY rep_name
                ");
            $sth2->bind_param(1, $rcfId);
            $sth2->execute;

            $rcf{"parameters"} = $sth2->fetchall_arrayref({});
            
            
            # load recorder input configs
            
            my $sth3 = $dbh->prepare("
                SELECT ric_identifier AS id, 
                    ric_index AS index,
                    ric_name AS name
                FROM RecorderInputConfig
                    INNER JOIN RecorderConfig ON (ric_recorder_conf_id = rec_identifier)
                WHERE
                    ric_recorder_conf_id = ?
                ORDER BY ric_index
                ");
            $sth3->bind_param(1, $rcfId);
            $sth3->execute;

            my @ricfs;            
            while (my $ricf = $sth3->fetchrow_hashref())
            {
                # load recorder input track configs
            
                my $sth4 = $dbh->prepare("
                    SELECT rtc_identifier AS id, 
                        rtc_index AS index,
                        rtc_track_number AS track_number,
                        rtc_source_id AS source_id,
                        scf_name AS source_name,
                        rtc_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM RecorderInputTrackConfig
                        INNER JOIN RecorderInputConfig ON (rtc_recorder_input_id = ric_identifier)
                        LEFT OUTER JOIN SourceConfig ON (rtc_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (rtc_source_id = sct_source_id AND
                            rtc_source_track_id = sct_track_id)
                    WHERE
                        rtc_recorder_input_id = ?
                    ORDER BY rtc_index
                    ");
                $sth4->bind_param(1, $ricf->{"ID"});
                $sth4->execute;
                
                push(@ricfs, {
                    config => $ricf,
                    tracks => $sth4->fetchall_arrayref({})
                });
            }
            
            $rcf{'inputs'} = \@ricfs;
        }
        else
        {
            $localError = "Failed to load recorder config with id $rcfId";
            die;
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%rcf;
}


sub save_recorder_config
{
    my ($dbh, $rcf) = @_;

    my $nextRecorderConfId;
    eval
    {
        $nextRecorderConfId = _load_next_id($dbh, "rec_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO RecorderConfig 
                (rec_identifier, rec_name, rec_recorder_id)
            VALUES
                (?, ?, ?)
            ");
        $sth->bind_param(1, $nextRecorderConfId, SQL_INTEGER);
        $sth->bind_param(2, $rcf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $rcf->{"config"}->{"RECORDER_ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $param (@{ $rcf->{"parameters"} })
        {
            my $nextParamId = _load_next_id($dbh, "rep_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO RecorderParameter 
                    (rep_identifier, rep_name, rep_value, rep_type, rep_recorder_conf_id)
                VALUES
                (?, ?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextParamId, SQL_INTEGER);
            $sth->bind_param(2, $param->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(3, $param->{"VALUE"}, SQL_VARCHAR);
            $sth->bind_param(4, $param->{"TYPE"}, SQL_INTEGER);
            $sth->bind_param(5, $nextRecorderConfId, SQL_INTEGER);
            $sth->execute;
        }
        
        foreach my $input (@{ $rcf->{"inputs"} })
        {
            my $nextInputId = _load_next_id($dbh, "ric_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO RecorderInputConfig 
                    (ric_identifier, ric_index, ric_name, ric_recorder_conf_id)
                VALUES
                (?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextInputId, SQL_INTEGER);
            $sth->bind_param(2, $input->{'config'}->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(3, $input->{'config'}->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(4, $nextRecorderConfId, SQL_INTEGER);
            $sth->execute;

            foreach my $track (@{ $input->{"tracks"} })
            {
                my $nextTrackId = _load_next_id($dbh, "rtc_id_seq");
            
                my $sth = $dbh->prepare("
                    INSERT INTO RecorderInputTrackConfig 
                        (rtc_identifier, rtc_index, rtc_source_id, rtc_source_track_id, 
                        rtc_track_number, rtc_recorder_input_id)
                    VALUES
                    (?, ?, ?, ?, ?, ?)
                    ");
                $sth->bind_param(1, $nextTrackId, SQL_INTEGER);
                $sth->bind_param(2, $track->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(3, $track->{"SOURCE_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $track->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(5, $track->{"TRACK_NUMBER"}, SQL_INTEGER);
                $sth->bind_param(6, $nextInputId, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextRecorderConfId;
}

sub update_recorder_config
{
    my ($dbh, $rcf) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE RecorderConfig
            SET
                rec_name = ?, 
                rec_recorder_id = ? 
            WHERE
                rec_identifier = ?
            ");
        $sth->bind_param(1, $rcf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $rcf->{"config"}->{"RECORDER_ID"}, SQL_INTEGER);
        $sth->bind_param(3, $rcf->{"config"}->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $param (@{ $rcf->{"parameters"} })
        {
            my $sth = $dbh->prepare("
                UPDATE RecorderParameter
                SET
                    rep_name = ?, 
                    rep_value = ?, 
                    rep_type = ?, 
                    rep_recorder_conf_id = ?
                WHERE
                    rep_identifier = ?
                ");
            $sth->bind_param(1, $param->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(2, $param->{"VALUE"}, SQL_VARCHAR);
            $sth->bind_param(3, $param->{"TYPE"}, SQL_INTEGER);
            $sth->bind_param(4, $rcf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(5, $param->{"ID"}, SQL_INTEGER);
            $sth->execute;
        }
        
        foreach my $input (@{ $rcf->{"inputs"} })
        {
            my $sth = $dbh->prepare("
                UPDATE RecorderInputConfig
                SET
                    ric_index = ?, 
                    ric_name = ?, 
                    ric_recorder_conf_id = ?
                WHERE
                    ric_identifier = ?
                ");
            $sth->bind_param(1, $input->{'config'}->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(2, $input->{'config'}->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(3, $rcf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(4, $input->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->execute;

            foreach my $track (@{ $input->{"tracks"} })
            {
                my $sth = $dbh->prepare("
                    UPDATE RecorderInputTrackConfig 
                    SET
                        rtc_index = ?, 
                        rtc_source_id = ?, 
                        rtc_source_track_id = ?, 
                        rtc_track_number = ?, 
                        rtc_recorder_input_id = ?
                    WHERE
                        rtc_identifier = ?
                    ");
                $sth->bind_param(1, $track->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(2, $track->{"SOURCE_ID"}, SQL_INTEGER);
                $sth->bind_param(3, $track->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $track->{"TRACK_NUMBER"}, SQL_INTEGER);
                $sth->bind_param(5, $input->{"config"}->{"ID"}, SQL_INTEGER);
                $sth->bind_param(6, $track->{"ID"}, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_recorder_config
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM RecorderConfig
            WHERE
                rec_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}


####################################
#
# Multi-camera configuration
#
####################################

sub load_multicam_configs
{
    my ($dbh) = @_;

    my @mccfs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT mcd_identifier AS id, 
                mcd_name AS name
            FROM MultiCameraClipDef
            ORDER BY mcd_name
            ");
        $sth->execute;

        while (my $mccf = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT mct_identifier AS id, 
                    mct_index AS index,
                    mct_track_number AS track_number
                FROM MultiCameraTrackDef
                    INNER JOIN MultiCameraClipDef ON (mct_multi_camera_def_id = mcd_identifier)
                WHERE
                    mct_multi_camera_def_id = ?
                ORDER BY mct_index
                ");
            $sth2->bind_param(1, $mccf->{"ID"});
            $sth2->execute;

            my @mctcfs;            
            while (my $mctcf = $sth2->fetchrow_hashref())
            {
                my $sth3 = $dbh->prepare("
                    SELECT mcs_identifier AS id, 
                        mcs_index AS index,
                        mcs_source_id AS source_id,
                        scf_name AS source_name,
                        mcs_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM MultiCameraSelectorDef
                        INNER JOIN MultiCameraTrackDef ON (mcs_multi_camera_track_def_id = mct_identifier)
                        LEFT OUTER JOIN SourceConfig ON (mcs_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (mcs_source_id = sct_source_id AND
                            mcs_source_track_id = sct_track_id)
                    WHERE
                        mcs_multi_camera_track_def_id = ?
                    ORDER BY mcs_index
                    ");
                $sth3->bind_param(1, $mctcf->{"ID"});
                $sth3->execute;
                
                push(@mctcfs, {
                    config => $mctcf,
                    selectors => $sth3->fetchall_arrayref({})
                });
            }
            
            push(@mccfs, {
                config => $mccf,
                tracks => \@mctcfs
                });
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@mccfs;
}

sub load_multicam_config
{
    my ($dbh, $mccfId) = @_;

    my $localError = "unknown error";
    my %mccf;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT mcd_identifier AS id, 
                mcd_name AS name
            FROM MultiCameraClipDef
            WHERE
                mcd_identifier = ?
            ");
        $sth->bind_param(1, $mccfId);
        $sth->execute;

        if ($mccf{"config"} = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT mct_identifier AS id, 
                    mct_index AS index,
                    mct_track_number AS track_number
                FROM MultiCameraTrackDef
                    INNER JOIN MultiCameraClipDef ON (mct_multi_camera_def_id = mcd_identifier)
                WHERE
                    mct_multi_camera_def_id = ?
                ORDER BY mct_index
                ");
            $sth2->bind_param(1, $mccfId);
            $sth2->execute;

            my @mctcfs;            
            while (my $mctcf = $sth2->fetchrow_hashref())
            {
                my $sth3 = $dbh->prepare("
                    SELECT mcs_identifier AS id, 
                        mcs_index AS index,
                        mcs_source_id AS source_id,
                        scf_name AS source_name,
                        mcs_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM MultiCameraSelectorDef
                        INNER JOIN MultiCameraTrackDef ON (mcs_multi_camera_track_def_id = mct_identifier)
                        LEFT OUTER JOIN SourceConfig ON (mcs_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (mcs_source_id = sct_source_id AND
                            mcs_source_track_id = sct_track_id)
                    WHERE
                        mcs_multi_camera_track_def_id = ?
                    ORDER BY mcs_index
                    ");
                $sth3->bind_param(1, $mctcf->{"ID"});
                $sth3->execute;
                
                push(@mctcfs, {
                    config => $mctcf,
                    selectors => $sth3->fetchall_arrayref({})
                });
            }
            
            $mccf{"tracks"} = \@mctcfs;
        }
        else
        {
            $localError = "Failed to load multi-camera config with id $mccfId";
            die;
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%mccf;
}

sub save_multicam_config
{
    my ($dbh, $mccf) = @_;

    my $nextMCId;
    eval
    {
        $nextMCId = _load_next_id($dbh, "mcd_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO MultiCameraClipDef 
                (mcd_identifier, mcd_name)
            VALUES
                (?, ?)
            ");
        $sth->bind_param(1, $nextMCId, SQL_INTEGER);
        $sth->bind_param(2, $mccf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->execute;
        
        foreach my $track (@{ $mccf->{"tracks"} })
        {
            my $nextTrackId = _load_next_id($dbh, "mct_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO MultiCameraTrackDef 
                    (mct_identifier, mct_index, mct_track_number, mct_multi_camera_def_id)
                VALUES
                (?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextTrackId, SQL_INTEGER);
            $sth->bind_param(2, $track->{'config'}->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(3, $track->{'config'}->{"TRACK_NUMBER"}, SQL_INTEGER);
            $sth->bind_param(4, $nextMCId, SQL_INTEGER);
            $sth->execute;

            foreach my $selector (@{ $track->{"selectors"} })
            {
                my $nextSelectorId = _load_next_id($dbh, "mcs_id_seq");
            
                my $sth = $dbh->prepare("
                    INSERT INTO MultiCameraSelectorDef 
                        (mcs_identifier, mcs_index, mcs_source_id, mcs_source_track_id, 
                        mcs_multi_camera_track_def_id)
                    VALUES
                    (?, ?, ?, ?, ?)
                    ");
                $sth->bind_param(1, $nextSelectorId, SQL_INTEGER);
                $sth->bind_param(2, $selector->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(3, $selector->{"SOURCE_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $selector->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(5, $nextTrackId, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextMCId;
}

sub update_multicam_config
{
    my ($dbh, $mccf) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE MultiCameraClipDef
            SET
                mcd_name = ?
            WHERE
                mcd_identifier = ?
            ");
        $sth->bind_param(1, $mccf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $mccf->{"config"}->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $mccf->{"tracks"} })
        {
            my $sth = $dbh->prepare("
                UPDATE MultiCameraTrackDef
                SET
                     mct_index = ?, 
                     mct_track_number = ?, 
                     mct_multi_camera_def_id = ?
                WHERE
                    mct_identifier = ?
                ");
            $sth->bind_param(1, $track->{'config'}->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(2, $track->{'config'}->{"TRACK_NUMBER"}, SQL_INTEGER);
            $sth->bind_param(3, $mccf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(4, $track->{'config'}->{"ID"}, SQL_INTEGER);
            $sth->execute;

            foreach my $selector (@{ $track->{"selectors"} })
            {
                my $sth = $dbh->prepare("
                    UPDATE MultiCameraSelectorDef
                    SET
                        mcs_index = ?, 
                        mcs_source_id = ?, 
                        mcs_source_track_id = ?, 
                        mcs_multi_camera_track_def_id = ?
                    WHERE
                        mcs_identifier = ?
                    ");
                $sth->bind_param(1, $selector->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(2, $selector->{"SOURCE_ID"}, SQL_INTEGER);
                $sth->bind_param(3, $selector->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $track->{'config'}->{"ID"}, SQL_INTEGER);
                $sth->bind_param(5, $selector->{"ID"}, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_multicam_config
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM MultiCameraClipDef
            WHERE
                mcd_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
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
                pkg_descriptor_id AS descriptor_id,
                opp_name AS op
            FROM Package
                LEFT OUTER JOIN ProjectName ON (pkg_project_name_id = pjn_identifier)
                LEFT OUTER JOIN OperationalPattern ON (pkg_op_id = opp_identifier)
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
                        eds_file_format AS file_format,
                        eds_video_resolution_id AS video_res_id,
                        vrn_name AS video_res,
                        (eds_image_aspect_ratio).numerator AS aspect_ratio_num,
                        (eds_image_aspect_ratio).denominator AS aspect_ratio_den,
                        eds_audio_quantization_bits AS audio_quant_bits,
                        eds_spool_number AS spool_number,
                        eds_recording_location AS recording_location_id
                    FROM EssenceDescriptor
                        LEFT OUTER JOIN VideoResolution ON (eds_video_resolution_id = vrn_identifier)
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
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
                or $localError = "Failed to load package with id = $packageRef->{'ID'}: $errstr" and die;

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
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
                            . "with id = $package->{'ID'}: $errstr" and die;
                }
            }
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
                or $localError = "Failed to load material package with id = $pkgId: $errstr" and die;
            $material{"materials"}->{ $materialPackage->{"UID"} } = $materialPackage;
            $material{"packages"}->{ $materialPackage->{"UID"} } = $materialPackage;

            # load the package chain referenced by the material package
            # load only the referenced file package and referenced tape/live source package
            load_package_chain($dbh, $materialPackage, $material{"packages"}, 0, 2)
                or $localError = "Failed to load package chain for material package with id=$pkgId: $errstr" and die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return -1;
    }
    
    return $count || 0;
}


####################################
#
# Proxy Definition
#
####################################

sub load_proxy_types
{
    my ($dbh) = @_;
    
    my $pxts;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pxt_identifier AS id, 
                pxt_name AS name
            FROM ProxyType
            ORDER BY pxt_identifier
            ");
        $sth->execute;
        
        $pxts = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $pxts;    
}

sub load_proxy_defs
{
    my ($dbh) = @_;

    my @pxdefs;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pxd_identifier AS id, 
                pxd_name AS name,
                pxd_type_id AS type_id,
                pxt_name AS type
            FROM ProxyDefinition
                INNER JOIN ProxyType ON (pxd_type_id = pxt_identifier)
            ORDER BY pxd_name
            ");
        $sth->execute;

        while (my $pxdef = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT ptd_identifier AS id, 
                    ptd_track_id AS track_id,
                    ptd_data_def_id AS data_def_id,
                    ddf_name AS data_def
                FROM ProxyTrackDefinition
                    INNER JOIN ProxyDefinition ON (ptd_proxy_def_id = pxd_identifier)
                    INNER JOIN DataDefinition ON (ptd_data_def_id = ddf_identifier)
                WHERE
                    ptd_proxy_def_id = ?
                ORDER BY ptd_track_id
                ");
            $sth2->bind_param(1, $pxdef->{"ID"});
            $sth2->execute;

            my @pxtdefs;            
            while (my $pxtdef = $sth2->fetchrow_hashref())
            {
                my $sth3 = $dbh->prepare("
                    SELECT psd_identifier AS id, 
                        psd_index AS index,
                        psd_source_id AS source_id,
                        scf_name AS source_name,
                        psd_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM ProxySourceDefinition
                        INNER JOIN ProxyTrackDefinition ON (psd_proxy_track_def_id = ptd_identifier)
                        LEFT OUTER JOIN SourceConfig ON (psd_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (psd_source_id = sct_source_id AND
                            psd_source_track_id = sct_track_id)
                    WHERE
                        psd_proxy_track_def_id = ?
                    ORDER BY psd_index
                    ");
                $sth3->bind_param(1, $pxtdef->{"ID"});
                $sth3->execute;
                
                push(@pxtdefs, {
                    def => $pxtdef,
                    sources => $sth3->fetchall_arrayref({})
                });
            }
            
            push(@pxdefs, {
                def => $pxdef,
                tracks => \@pxtdefs
                });
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@pxdefs;
}

sub load_proxy_def
{
    my ($dbh, $pxdefId) = @_;

    my $localError = "unknown error";
    my %pxdef;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pxd_identifier AS id, 
                pxd_name AS name,
                pxd_type_id AS type_id,
                pxt_name AS type
            FROM ProxyDefinition
                INNER JOIN ProxyType ON (pxd_type_id = pxt_identifier)
            WHERE
                pxd_identifier = ?
            ");
        $sth->bind_param(1, $pxdefId);
        $sth->execute;

        if ($pxdef{"def"} = $sth->fetchrow_hashref())
        {
            my $sth2 = $dbh->prepare("
                SELECT ptd_identifier AS id, 
                    ptd_track_id AS track_id,
                    ptd_data_def_id AS data_def_id,
                    ddf_name AS data_def
                FROM ProxyTrackDefinition
                    INNER JOIN ProxyDefinition ON (ptd_proxy_def_id = pxd_identifier)
                    INNER JOIN DataDefinition ON (ptd_data_def_id = ddf_identifier)
                WHERE
                    ptd_proxy_def_id = ?
                ORDER BY ptd_track_id
                ");
            $sth2->bind_param(1, $pxdefId);
            $sth2->execute;

            my @pxtdefs;            
            while (my $pxtdef = $sth2->fetchrow_hashref())
            {
                my $sth3 = $dbh->prepare("
                    SELECT psd_identifier AS id, 
                        psd_index AS index,
                        psd_source_id AS source_id,
                        scf_name AS source_name,
                        psd_source_track_id AS source_track_id,
                        sct_track_name AS source_track_name
                    FROM ProxySourceDefinition
                        INNER JOIN ProxyTrackDefinition ON (psd_proxy_track_def_id = ptd_identifier)
                        LEFT OUTER JOIN SourceConfig ON (psd_source_id = scf_identifier)
                        LEFT OUTER JOIN SourceTrackConfig ON (psd_source_id = sct_source_id AND
                            psd_source_track_id = sct_track_id)
                    WHERE
                        psd_proxy_track_def_id = ?
                    ORDER BY psd_index
                    ");
                $sth3->bind_param(1, $pxtdef->{"ID"});
                $sth3->execute;
                
                push(@pxtdefs, {
                    def => $pxtdef,
                    sources => $sth3->fetchall_arrayref({})
                });
            }
            
            $pxdef{"tracks"} = \@pxtdefs;
        }
        else
        {
            $localError = "Failed to load proxy definition with id $pxdefId";
            die;
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%pxdef;
}

sub save_proxy_def
{
    my ($dbh, $pxdef) = @_;

    my $nextPxdId;
    eval
    {
        $nextPxdId = _load_next_id($dbh, "pxd_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO ProxyDefinition 
                (pxd_identifier, pxd_name, pxd_type_id)
            VALUES
                (?, ?, ?)
            ");
        $sth->bind_param(1, $nextPxdId, SQL_INTEGER);
        $sth->bind_param(2, $pxdef->{"def"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $pxdef->{"def"}->{"TYPE_ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $pxdef->{"tracks"} })
        {
            my $nextTrackId = _load_next_id($dbh, "ptd_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO ProxyTrackDefinition 
                    (ptd_identifier, ptd_track_id, ptd_data_def_id, ptd_proxy_def_id)
                VALUES
                    (?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextTrackId, SQL_INTEGER);
            $sth->bind_param(2, $track->{"def"}->{"TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(3, $track->{"def"}->{"DATA_DEF_ID"}, SQL_INTEGER);
            $sth->bind_param(4, $nextPxdId, SQL_INTEGER);
            $sth->execute;

            foreach my $source (@{ $track->{"sources"} })
            {
                my $nextSourceId = _load_next_id($dbh, "psd_id_seq");
            
                my $sth = $dbh->prepare("
                    INSERT INTO ProxySourceDefinition 
                        (psd_identifier, psd_index, psd_source_id, psd_source_track_id, 
                        psd_proxy_track_def_id)
                    VALUES
                    (?, ?, ?, ?, ?)
                    ");
                $sth->bind_param(1, $nextSourceId, SQL_INTEGER);
                $sth->bind_param(2, $source->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(3, $source->{"SOURCE_ID"}, SQL_VARCHAR);
                $sth->bind_param(4, $source->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(5, $nextTrackId, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextPxdId;
}

sub update_proxy_def
{
    my ($dbh, $pxdef) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE ProxyDefinition
            SET
                pxd_name = ?,
                pxd_type_id = ?
            WHERE
                pxd_identifier = ?
            ");
        $sth->bind_param(1, $pxdef->{"def"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $pxdef->{"def"}->{"TYPE_ID"}, SQL_INTEGER);
        $sth->bind_param(3, $pxdef->{"def"}->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $pxdef->{"tracks"} })
        {
            my $sth = $dbh->prepare("
                UPDATE ProxyTrackDefinition
                SET
                     ptd_track_id = ?, 
                     ptd_data_def_id = ?, 
                     ptd_proxy_def_id = ?
                WHERE
                    ptd_identifier = ?
                ");
            $sth->bind_param(1, $track->{"def"}->{"TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(2, $track->{"def"}->{"DATA_DEF_ID"}, SQL_INTEGER);
            $sth->bind_param(3, $pxdef->{"def"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(4, $track->{"def"}->{"ID"}, SQL_INTEGER);
            $sth->execute;

            foreach my $source (@{ $track->{"sources"} })
            {
                my $sth = $dbh->prepare("
                    UPDATE ProxySourceDefinition
                    SET
                        psd_index = ?, 
                        psd_source_id = ?, 
                        psd_source_track_id = ?, 
                        psd_proxy_track_def_id = ?
                    WHERE
                        psd_identifier = ?
                    ");
                $sth->bind_param(1, $source->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(2, $source->{"SOURCE_ID"}, SQL_VARCHAR);
                $sth->bind_param(3, $source->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $track->{"def"}->{"ID"}, SQL_INTEGER);
                $sth->bind_param(5, $source->{"ID"}, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_proxy_def
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM ProxyDefinition
            WHERE
                pxd_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}



####################################
#
# Proxy
#
####################################

sub load_material_for_proxy
{
    my ($dbh) = @_;

    my %material;
    
    my $localError = "unknown error";
    eval
    {
        # load the next 100 material packages
        my $sth = $dbh->prepare("
            SELECT 
                mpx_identifier AS id,
                mpx_material_package_id AS material_package_id,
                mpx_timestamp AS tstamp
            FROM MaterialForProxy
            ORDER BY mpx_timestamp ASC
            ");
        $sth->execute;

        while (my $pkg = $sth->fetchrow_hashref())
        {
            my $pkgId = $pkg->{"MATERIAL_PACKAGE_ID"};
            
            # load the whole material package
            my $materialPackage = load_package($dbh, $pkgId)
                or $localError = "Failed to load material package with id = $pkgId: $errstr" and die;
            $materialPackage->{"pxytimestamp"} = $pkg->{"TSTAMP"};                
            $material{"materials"}->{ $materialPackage->{"UID"} } = $materialPackage;
            $material{"packages"}->{ $materialPackage->{"UID"} } = $materialPackage;

            # load the package chain referenced by the material package
            # load only the referenced file package and referenced tape/live source package
            load_package_chain($dbh, $materialPackage, $material{"packages"}, 0, 2)
                or $localError = "Failed to load package chain for material package with id=$pkgId: $errstr" and die;
        }
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%material;
}

sub delete_material_for_proxy
{
    my ($dbh, $timestampDiff) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM MaterialForProxy
            WHERE
                mpx_timestamp < now() - '$timestampDiff seconds'::interval
            ");
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub have_proxy
{
    my ($dbh, $proxy) = @_;

    my $haveProxy = 0;
    my $localError = "unknown error";
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pxy_identifier 
            FROM Proxy
            WHERE
                pxy_name = ? AND
                pxy_type_id = ? AND
                pxy_start_date = ? AND
                pxy_start_time = ?
            ");
        $sth->bind_param(1, $proxy->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $proxy->{"TYPE_ID"}, SQL_INTEGER);
        $sth->bind_param(3, $proxy->{"START_DATE"}, SQL_VARCHAR);
        $sth->bind_param(4, $proxy->{"START_TIME"}, SQL_INTEGER);
        $sth->execute;
        
        $haveProxy = ($sth->rows > 0);
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return 0;
    }
    
    return $haveProxy;
}

sub save_proxy
{
    my ($dbh, $proxy) = @_;

    my $nextPxId;
    eval
    {
        $nextPxId = _load_next_id($dbh, "pxy_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Proxy 
                (pxy_identifier, pxy_name, pxy_type_id, pxy_start_date, pxy_start_time,
                    pxy_status_id)
            VALUES
                (?, ?, ?, ?, ?, ?)
            ");
        $sth->bind_param(1, $nextPxId, SQL_INTEGER);
        $sth->bind_param(2, $proxy->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $proxy->{"TYPE_ID"}, SQL_INTEGER);
        $sth->bind_param(4, $proxy->{"START_DATE"}, SQL_VARCHAR);
        $sth->bind_param(5, $proxy->{"START_TIME"}, SQL_INTEGER);
        $sth->bind_param(6, $proxy->{"STATUS_ID"}, SQL_INTEGER);
        $sth->execute;
        
        foreach my $track (@{ $proxy->{"tracks"} })
        {
            my $nextTrackId = _load_next_id($dbh, "pyt_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO ProxyTrack 
                    (pyt_identifier, pyt_track_id, pyt_data_def_id, pyt_proxy_id)
                VALUES
                (?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextTrackId, SQL_INTEGER);
            $sth->bind_param(2, $track->{"TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(3, $track->{"DATA_DEF_ID"}, SQL_INTEGER);
            $sth->bind_param(4, $nextPxId, SQL_INTEGER);
            $sth->execute;

            foreach my $source (@{ $track->{"sources"} })
            {
                my $nextSourceId = _load_next_id($dbh, "pys_id_seq");
            
                my $sth = $dbh->prepare("
                    INSERT INTO ProxySource 
                        (pys_identifier, pys_index, pys_source_material_package_uid, 
                            pys_source_material_track_id, pys_source_file_package_uid, 
                            pys_source_file_track_id, pys_filename, pys_proxy_track_id)
                    VALUES
                    (?, ?, ?, ?, ?, ?, ?, ?)
                    ");
                $sth->bind_param(1, $nextSourceId, SQL_INTEGER);
                $sth->bind_param(2, $source->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(3, $source->{"SOURCE_MATERIAL_PACKAGE_UID"}, SQL_VARCHAR);
                $sth->bind_param(4, $source->{"SOURCE_MATERIAL_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(5, $source->{"SOURCE_FILE_PACKAGE_UID"}, SQL_VARCHAR);
                $sth->bind_param(6, $source->{"SOURCE_FILE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(7, $source->{"FILENAME"}, SQL_VARCHAR);
                $sth->bind_param(8, $nextTrackId, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextPxId;
}




####################################
#
# Router configuration
#
####################################

sub load_router_configs
{
    my ($dbh) = @_;

    my @rocfs;
    eval
    {
        my $sth;
        
        $sth = $dbh->prepare("
            SELECT ror_identifier AS id, 
                ror_name AS name
            FROM RouterConfig
            ORDER BY ror_name
            ");
        $sth->execute;

        while (my $rocf = $sth->fetchrow_hashref())
        {
            # inputs
            
            my $sth2 = $dbh->prepare("
                SELECT rti_identifier AS id, 
                    rti_index AS index,
                    rti_name AS name,
                    rti_source_id AS source_id,
                    rti_source_track_id AS source_track_id,
                    scf_name AS source_name,
                    sct_track_name AS source_track_name
                FROM RouterInputConfig
                    INNER JOIN RouterConfig ON (rti_router_conf_id = ror_identifier)
                    LEFT OUTER JOIN SourceConfig ON (rti_source_id = scf_identifier)
                    LEFT OUTER JOIN SourceTrackConfig ON (rti_source_id = sct_source_id AND
                        rti_source_track_id = sct_track_id)
                WHERE
                    rti_router_conf_id = ?
                ORDER BY rti_index
                ");
            $sth2->bind_param(1, $rocf->{"ID"});
            $sth2->execute;

            
            # outputs
            
            my $sth3 = $dbh->prepare("
                SELECT rto_identifier AS id, 
                    rto_index AS index,
                    rto_name AS name
                FROM RouterOutputConfig
                    INNER JOIN RouterConfig ON (rto_router_conf_id = ror_identifier)
                WHERE
                    rto_router_conf_id = ?
                ORDER BY rto_index
                ");
            $sth3->bind_param(1, $rocf->{"ID"});
            $sth3->execute;

            
            push(@rocfs, {
                config => $rocf,
                inputs => $sth2->fetchall_arrayref({}),
                outputs => $sth3->fetchall_arrayref({}),
                });
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@rocfs;
}

sub load_router_config
{
    my ($dbh, $rocfId) = @_;

    my $localError = "unknown error";
    my %rocf;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT ror_identifier AS id, 
                ror_name AS name
            FROM RouterConfig
            WHERE
                ror_identifier = ?
            ORDER BY ror_name
            ");
        $sth->bind_param(1, $rocfId);
        $sth->execute;

        if ($rocf{'config'} = $sth->fetchrow_hashref())
        {
            # inputs
            
            my $sth2 = $dbh->prepare("
                SELECT rti_identifier AS id, 
                    rti_index AS index,
                    rti_name AS name,
                    rti_source_id AS source_id,
                    rti_source_track_id AS source_track_id,
                    scf_name AS source_name,
                    sct_track_name AS source_track_name
                FROM RouterInputConfig
                    INNER JOIN RouterConfig ON (rti_router_conf_id = ror_identifier)
                    LEFT OUTER JOIN SourceConfig ON (rti_source_id = scf_identifier)
                    LEFT OUTER JOIN SourceTrackConfig ON (rti_source_id = sct_source_id AND
                        rti_source_track_id = sct_track_id)
                WHERE
                    rti_router_conf_id = ?
                ORDER BY rti_index
                ");
            $sth2->bind_param(1, $rocfId);
            $sth2->execute;

            
            # outputs
            
            my $sth3 = $dbh->prepare("
                SELECT rto_identifier AS id, 
                    rto_index AS index,
                    rto_name AS name
                FROM RouterOutputConfig
                    INNER JOIN RouterConfig ON (rto_router_conf_id = ror_identifier)
                WHERE
                    rto_router_conf_id = ?
                ORDER BY rto_index
                ");
            $sth3->bind_param(1, $rocfId);
            $sth3->execute;

            
            $rocf{'inputs'} = $sth2->fetchall_arrayref({});
            $rocf{'outputs'} = $sth3->fetchall_arrayref({});
        }
        else
        {
            $localError = "Failed to load router config with id $rocfId";
            die;
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return \%rocf;
}


sub save_router_config
{
    my ($dbh, $rocf) = @_;

    my $nextRouterConfId;
    eval
    {
        $nextRouterConfId = _load_next_id($dbh, "ror_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO RouterConfig 
                (ror_identifier, ror_name)
            VALUES
                (?, ?)
            ");
        $sth->bind_param(1, $nextRouterConfId, SQL_INTEGER);
        $sth->bind_param(2, $rocf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->execute;
        
        # inputs
        
        foreach my $input (@{ $rocf->{"inputs"} })
        {
            my $nextInputId = _load_next_id($dbh, "rti_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO RouterInputConfig 
                    (rti_identifier, rti_index, rti_name, rti_source_id, rti_source_track_id, rti_router_conf_id)
                VALUES
                (?, ?, ?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextInputId, SQL_INTEGER);
            $sth->bind_param(2, $input->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(3, $input->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(4, $input->{"SOURCE_ID"}, SQL_INTEGER);
            $sth->bind_param(5, $input->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(6, $nextRouterConfId, SQL_INTEGER);
            $sth->execute;
        }
        
        # outputs
        
        foreach my $output (@{ $rocf->{"outputs"} })
        {
            my $nextOutputId = _load_next_id($dbh, "rto_id_seq");
        
            my $sth = $dbh->prepare("
                INSERT INTO RouterOutputConfig 
                    (rto_identifier, rto_index, rto_name, rto_router_conf_id)
                VALUES
                (?, ?, ?, ?)
                ");
            $sth->bind_param(1, $nextOutputId, SQL_INTEGER);
            $sth->bind_param(2, $output->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(3, $output->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(4, $nextRouterConfId, SQL_INTEGER);
            $sth->execute;
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextRouterConfId;
}

sub update_router_config
{
    my ($dbh, $rocf) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE RouterConfig
            SET
                ror_name = ? 
            WHERE
                ror_identifier = ?
            ");
        $sth->bind_param(1, $rocf->{"config"}->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $rocf->{"config"}->{"ID"}, SQL_INTEGER);
        $sth->execute;
        
        # inputs
        
        foreach my $input (@{ $rocf->{"inputs"} })
        {
            my $sth = $dbh->prepare("
                UPDATE RouterInputConfig
                SET
                    rti_index = ?, 
                    rti_name = ?, 
                    rti_source_id = ?, 
                    rti_source_track_id = ?, 
                    rti_router_conf_id = ?
                WHERE
                    rti_identifier = ?
                ");
            $sth->bind_param(1, $input->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(2, $input->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(3, $input->{"SOURCE_ID"}, SQL_INTEGER);
            $sth->bind_param(4, $input->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
            $sth->bind_param(5, $rocf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(6, $input->{"ID"}, SQL_INTEGER);
            $sth->execute;
        }
        
        # outputs
        
        foreach my $output (@{ $rocf->{"outputs"} })
        {
            my $sth = $dbh->prepare("
                UPDATE RouterOutputConfig
                SET
                    rto_index = ?, 
                    rto_name = ?, 
                    rto_router_conf_id = ?
                WHERE
                    rto_identifier = ?
                ");
            $sth->bind_param(1, $output->{"INDEX"}, SQL_INTEGER);
            $sth->bind_param(2, $output->{"NAME"}, SQL_VARCHAR);
            $sth->bind_param(3, $rocf->{"config"}->{"ID"}, SQL_INTEGER);
            $sth->bind_param(4, $output->{"ID"}, SQL_INTEGER);
            $sth->execute;
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub delete_router_config
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM RouterConfig
            WHERE
                ror_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}




1;

