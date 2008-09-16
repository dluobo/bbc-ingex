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
use lib "../../ingex-config/";
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
        &load_router_configs
        &load_router_config
        &save_router_config
        &update_router_config
        &delete_router_config
		&load_projects
		&load_project
        &save_project 
        &update_project 
        &delete_project
    );

    @EXPORT_OK = qw(
        %sourceType 
        %dataDef 
    );
}

####################################
#
# Exported package globals
#
####################################

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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextSourceId = prodautodb::_load_next_id($dbh, "scf_id_seq");
        
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
            my $nextTrackId = prodautodb::_load_next_id($dbh, "sct_id_seq");
        
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
            my $nextTrackId = prodautodb::_load_next_id($dbh, "sct_id_seq");
        
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextRecorderId = prodautodb::_load_next_id($dbh, "rer_id_seq");
        
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextRecorderConfId = prodautodb::_load_next_id($dbh, "rec_id_seq");
        
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
            my $nextParamId = prodautodb::_load_next_id($dbh, "rep_id_seq");
        
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
            my $nextInputId = prodautodb::_load_next_id($dbh, "ric_id_seq");
        
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
                my $nextTrackId = prodautodb::_load_next_id($dbh, "rtc_id_seq");
            
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextMCId = prodautodb::_load_next_id($dbh, "mcd_id_seq");
        
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
            my $nextTrackId = prodautodb::_load_next_id($dbh, "mct_id_seq");
        
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
                my $nextSelectorId = prodautodb::_load_next_id($dbh, "mcs_id_seq");
            
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextPxdId = prodautodb::_load_next_id($dbh, "pxd_id_seq");
        
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
            my $nextTrackId = prodautodb::_load_next_id($dbh, "ptd_id_seq");
        
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
                my $nextSourceId = prodautodb::_load_next_id($dbh, "psd_id_seq");
            
                my $sth = $dbh->prepare("
                    INSERT INTO ProxySourceDefinition 
                        (psd_identifier, psd_index, psd_source_id, psd_source_track_id, 
                        psd_proxy_track_def_id)
                    VALUES
                    (?, ?, ?, ?, ?)
                    ");
                $sth->bind_param(1, $nextSourceId, SQL_INTEGER);
                $sth->bind_param(2, $source->{"INDEX"}, SQL_INTEGER);
                $sth->bind_param(3, $source->{"SOURCE_ID"}, SQL_INTEGER);
                $sth->bind_param(4, $source->{"SOURCE_TRACK_ID"}, SQL_INTEGER);
                $sth->bind_param(5, $nextTrackId, SQL_INTEGER);
                $sth->execute;
            }
        }
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
                $sth->bind_param(2, $source->{"SOURCE_ID"}, SQL_INTEGER);
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
                or $localError = "Failed to load material package with id = $pkgId: $prodautodb::errstr" and die;
            $materialPackage->{"pxytimestamp"} = $pkg->{"TSTAMP"};                
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextPxId = prodautodb::_load_next_id($dbh, "pxy_id_seq");
        
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
            my $nextTrackId = prodautodb::_load_next_id($dbh, "pyt_id_seq");
        
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
                my $nextSourceId = prodautodb::_load_next_id($dbh, "pys_id_seq");
            
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextRouterConfId = prodautodb::_load_next_id($dbh, "ror_id_seq");
        
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
            my $nextInputId = prodautodb::_load_next_id($dbh, "rti_id_seq");
        
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
            my $nextOutputId = prodautodb::_load_next_id($dbh, "rto_id_seq");
        
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

####################################
#
# Project Names
#
####################################

sub load_projects
{
    my ($dbh) = @_;
    
    my $proj;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pjn_identifier AS id, 
                pjn_name AS name
            FROM Projectname
            ");
        $sth->execute;
        
        $proj = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $proj;    
}

sub load_project
{
    my ($dbh, $Id) = @_;
    
    my $localError = "unknown error";
    my $proj;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT pjn_identifier AS id, 
                pjn_name AS name
            FROM ProjectName
            WHERE
                pjn_identifier = ?
            ");
        $sth->bind_param(1, $Id, SQL_INTEGER);
        $sth->execute;
        
        unless ($proj = $sth->fetchrow_hashref())
        {
            $localError = "Failed to load project name with id $Id";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $proj;    
}

sub save_project
{
    my ($dbh, $name) = @_;

    my $nextId;
    eval
    {
        $nextId = prodautodb::_load_next_id($dbh, "pjn_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO ProjectName 
                (pjn_identifier, pjn_name)
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
        $prodautodb::errstr = $@;#$dbh->errstr;
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_project
{
    my ($dbh, $proj) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE ProjectName
            SET
                pjn_name = ?
            WHERE
                pjn_identifier = ?
            ");
        $sth->bind_param(1, $proj->{'NAME'}, SQL_VARCHAR);
        $sth->bind_param(2, $proj->{'ID'}, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = $dbh->errstr;
        eval { $dbh->rollback; };
        return 0;
    }

    return 1;    
}

sub delete_project
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM ProjectName
            WHERE
                pjn_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

####################################
#
# Series
#
####################################

sub load_series
{
    my ($dbh) = @_;
    
    my $series;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT srs_identifier AS id, 
                srs_name AS name
            FROM Series
            ");
        $sth->execute;
        
        $series = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $series;    
}

sub load_one_series
{
    my ($dbh, $Id) = @_;
    
    my $localError = "unknown error";
    my $series;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT srs_identifier AS id,
                srs_name AS name
            FROM Series
            WHERE
                srs_identifier = ?
            ");
        $sth->bind_param(1, $Id, SQL_INTEGER);
        $sth->execute;
        
        unless ($series = $sth->fetchrow_hashref())
        {
            $localError = "Failed to load series with id $Id";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $series;
}

sub save_series
{
    my ($dbh, $name) = @_;

    my $nextId;
    eval
    {
        $nextId = prodautodb::_load_next_id($dbh, "srs_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Series
                (srs_identifier, srs_name)
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
        $prodautodb::errstr = $@;#$dbh->errstr;
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_series
{
    my ($dbh, $series) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE Series
            SET
                srs_name = ?
            WHERE
                srs_identifier = ?
            ");
        $sth->bind_param(1, $series->{'NAME'}, SQL_VARCHAR);
        $sth->bind_param(2, $series->{'ID'}, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = $dbh->errstr;
        eval { $dbh->rollback; };
        return 0;
    }

    return 1;    
}

sub delete_series
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM Series
            WHERE
                srs_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

####################################
#
# Programmes
#
####################################

sub load_programmes
{
    my ($dbh) = @_;
    
    my $prog;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT prg_identifier AS id, 
                prg_name AS name,
				srs_name AS series
            FROM Programme
			LEFT OUTER JOIN Series ON (prg_series_id = srs_identifier)
            ");
        $sth->execute;
        
        $prog = $sth->fetchall_arrayref({});
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $prog;    
}

sub load_programme
{
    my ($dbh, $Id) = @_;
    
    my $localError = "unknown error";
    my $prog;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT prg_identifier AS id,
               prg_name AS name,
			   srs_name AS series,
			   prg_series_id AS seriesId
            FROM Programme
			LEFT OUTER JOIN Series ON (prg_series_id = srs_identifier)
            WHERE
                prg_identifier = ?
            ");
        $sth->bind_param(1, $Id, SQL_INTEGER);
        $sth->execute;
        
        unless ($prog = $sth->fetchrow_hashref())
        {
            $localError = "Failed to load programme with id $Id";
            die;
        }
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }
    
    return $prog;
}

sub save_programme
{
    my ($dbh, $name, $series) = @_;

    my $nextId;
    eval
    {
        $nextId = prodautodb::_load_next_id($dbh, "prg_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Programme
                (prg_identifier, prg_name, prg_series_id)
            VALUES
                (?, ?, ?)
            ");
        $sth->bind_param(1, $nextId, SQL_INTEGER);
        $sth->bind_param(2, $name, SQL_VARCHAR);
		$sth->bind_param(3, $series, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = $@;#$dbh->errstr;
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_programme
{
    my ($dbh, $prog) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE Programme
            SET
                prg_name = ?,
				prg_series_id = ?
            WHERE
                prg_identifier = ?
            ");
        $sth->bind_param(1, $prog->{'NAME'}, SQL_VARCHAR);
        $sth->bind_param(3, $prog->{'ID'}, SQL_INTEGER);
		$sth->bind_param(2, $prog->{'SERIESID'}, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = $dbh->errstr;
        eval { $dbh->rollback; };
        return 0;
    }

    return 1;    
}

sub delete_programme
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM Programme
            WHERE
                prg_identifier = ?
            ");
        $sth->bind_param(1, $id, SQL_INTEGER);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}



1;

