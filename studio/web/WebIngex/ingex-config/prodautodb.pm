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
 
package prodautodb;

use strict;
use warnings;
use lib ".";
use ingexhtmlutil;

use DBI qw(:sql_types);
use DBD::Pg qw(:pg_types);

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
		&load_recording_locations
		&load_recording_location
        &save_recording_location
        &update_recording_location
        &delete_recording_location
		&load_items
		&load_item
		&save_item
		&update_item
		&delete_item
		&load_takes
		&load_take
		&save_take
        &dump_table
        &load_video_resolutions
        &load_video_resolution
		&load_nodes
		&load_node
		&save_node
		&update_node
		&delete_node
		&load_node_types
    );

    @EXPORT_OK = qw(
        &connect 
        &disconnect 
        $errstr
    );
}

####################################
#
# Exported package globals
#
####################################

our $errstr = "";

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
		return_error_page("Failed to connect to database: $host<br />Error: $errstr");
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
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
        $nextId = prodautodb::_load_next_id($dbh, "rlc_id_seq");
        
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
        $prodautodb::errstr = $@;#$dbh->errstr;
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
        $prodautodb::errstr = $dbh->errstr;
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
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}


####################################
#
# Node configuration
#
####################################

sub load_nodes
{
    my ($dbh) = @_;

    my @nodes;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT node_id AS id, 
                node_name AS name,
                node_type AS type,
				node_ip AS ip,
				node_volumes AS volumes
			FROM Nodes
            ORDER BY node_name
            ");
        $sth->execute;

        while (my $node = $sth->fetchrow_hashref())
        {            
            push(@nodes, $node);
        }
    
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@nodes;
}

sub load_node
{
    my ($dbh, $nodeId) = @_;

    my $localError = "unknown error";
    my $node;
    eval
    {
        my $sth = $dbh->prepare("
        	SELECT node_id AS id, 
            	node_name AS name,
                node_type AS type,
				node_ip AS ip,
				node_volumes AS volumes
         	FROM Nodes
            WHERE
                node_id = ?
            ");
        $sth->bind_param(1, $nodeId);
        $sth->execute;
		$node = $sth->fetchrow_hashref();
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : $localError;
        return undef;
    }

    return $node;
}

sub save_node
{
    my ($dbh, $node) = @_;

    my $nextNodeId;
    eval
    {
        $nextNodeId = _load_next_id($dbh, "node_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Nodes 
                (node_id, node_name, node_type, node_ip, node_volumes)
            VALUES
                (?, ?, ?, ?, ?)
            ");
        $sth->bind_param(1, $nextNodeId, SQL_INTEGER);
        $sth->bind_param(2, $node->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(3, $node->{"TYPE"}, SQL_VARCHAR);
        $sth->bind_param(4, $node->{"IP"}, SQL_VARCHAR);
		$sth->bind_param(5, $node->{"VOLUMES"}, SQL_VARCHAR);
        $sth->execute;
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextNodeId;
}

sub update_node
{
    my ($dbh, $node) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE Nodes
            SET
                node_name = ?, 
                node_type = ?, 
                node_ip = ?,
				node_volumes = ?
            WHERE
                node_id = ?
            ");
        $sth->bind_param(1, $node->{"NAME"}, SQL_VARCHAR);
        $sth->bind_param(2, $node->{"TYPE"}, SQL_VARCHAR);
        $sth->bind_param(3, $node->{"IP"}, SQL_VARCHAR);
		$sth->bind_param(4, $node->{"VOLUMES"}, SQL_VARCHAR);
        $sth->bind_param(5, $node->{"ID"}, SQL_INTEGER);
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

sub delete_node
{
    my ($dbh, $id) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM Nodes
            WHERE
                node_id = ?
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
# Items & Takes
#
####################################

sub load_items
{
    my ($dbh,$progId) = @_;

    my @items;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT itm_identifier AS id,
 				itm_order_index AS orderIndex,
                itm_description AS itemName,
				itm_script_section_ref AS sequence
			FROM Item
			WHERE itm_programme_id = ?
            ORDER BY itm_identifier
            ");
		$sth->bind_param(1, $progId);
        $sth->execute;

        while (my $item = $sth->fetchrow_hashref())
        {            
            push(@items, $item);
        }
    
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@items;
}

sub load_item
{
    my ($dbh,$itemID) = @_;
	my $item;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT itm_identifier AS id,
 				itm_order_index AS orderIndex,
                itm_description AS itemName,
				itm_script_section_ref AS sequence,
				itm_programme_id AS programme
			FROM Item
			WHERE itm_identifier = ?
            ");
		$sth->bind_param(1, $itemID);
        $sth->execute;

        $item = $sth->fetchrow_hashref()
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $item;
}

sub save_item
{
    my ($dbh, $x) = @_;

    my $nextId;
    eval
    {
        $nextId = _load_next_id($dbh, "itm_id_seq");

        my $sth = $dbh->prepare("
            INSERT INTO Item
                (itm_identifier, itm_description, itm_script_section_ref, itm_order_index, itm_programme_id)
            VALUES
                (?, ?, ?, ?, ?)
            ");
        $sth->execute($nextId, $x->{'ITEMNAME'}, $x->{'SEQUENCE'}, $x->{'ORDERINDEX'}, $x->{'PROGRAMME'});
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "database error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_item
{
    my ($dbh, $x) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            UPDATE Item
				SET
					itm_description = ?,
					itm_script_section_ref = ?,
					itm_order_index = ?,
					itm_programme_id = ?
            	WHERE
					itm_identifier = ?
            ");
        $sth->execute($x->{'ITEMNAME'}, $x->{'SEQUENCE'}, $x->{'ORDERINDEX'}, $x->{'PROGRAMME'}, $x->{'ID'});
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "database error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return $x->{'ID'};
}

sub delete_item
{
    my ($dbh, $x) = @_;

    eval
    {
        my $sth = $dbh->prepare("
            DELETE FROM Item
				WHERE itm_identifier = ?
            ");
        $sth->execute($x);
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "database error";
        eval { $dbh->rollback; };
        return 0;
    }
    
    return 1;
}

sub load_takes
{
    my ($dbh, $itemId) = @_;

    my @takes;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT tke_identifier AS id, 
                tke_number AS takeNo,
				tke_comment AS comment,
				tkr_name AS result,
				tke_length AS length,
				tke_start_position AS start,
				tke_start_date AS date,
				rlc_name AS location,
				tke_edit_rate AS editrate
			FROM Take
			LEFT OUTER JOIN RecordingLocation ON (tke_recording_location = rlc_identifier)
			LEFT OUTER JOIN TakeResult ON (tke_result = tkr_identifier)
			WHERE tke_item_id = ?
            ORDER BY tke_number
            ");
		$sth->bind_param(1, $itemId);
        $sth->execute;

        while (my $take = $sth->fetchrow_hashref())
        {            
            push(@takes, $take);
        }
    
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return \@takes;
}

sub load_take
{
    my ($dbh, $id) = @_;

    my $take;
    eval
    {
        my $sth = $dbh->prepare("
            SELECT tke_identifier AS id, 
                tke_number AS takeNo,
				tke_comment AS comment,
				tke_result AS resultid,
				tkr_name AS result,
				tke_length AS length,
				tke_start_position AS start,
				tke_start_date AS date,
				tke_edit_rate AS editrate,
				tke_item_id AS item,
				tke_recording_location AS locationid,
				rlc_name AS location
			FROM Take
			LEFT OUTER JOIN RecordingLocation ON (tke_recording_location = rlc_identifier)
			LEFT OUTER JOIN TakeResult ON (tke_result = tkr_identifier)
			WHERE tke_identifier = ?
            ORDER BY tke_number
            ");
		$sth->bind_param(1, $id);
        $sth->execute;
        $take = $sth->fetchrow_hashref();
    };
    if ($@)
    {
        $prodautodb::errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        return undef;
    }
    
    return $take;
}

sub save_take
{
    my ($dbh, $x) = @_;

    my $nextId;
    eval
    {
        $nextId = _load_next_id($dbh, "tke_id_seq");
        
        my $sth = $dbh->prepare("
            INSERT INTO Take
                (tke_identifier, tke_number, tke_recording_location, tke_start_date, tke_start_position, tke_length, tke_result, tke_comment, tke_item_id, tke_edit_rate)
            VALUES
                (?, ?, ?, ?, ?, ?, ?, ?, ?, (?,?))
            ");
        $sth->execute($nextId, $x->{"TAKENO"}, $x->{"LOCATION"}, $x->{"DATE"}, $x->{"START"}, $x->{"LENGTH"}, $x->{"RESULT"}, $x->{"COMMENT"}, $x->{"ITEM"}, $x->{"EDITRATE"}, $x->{"EDITRATEDENOM"},);
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "INSERT INTO Take (tke_identifier, tke_number, tke_recording_location, tke_start_date, tke_start_position, tke_length, tke_result, tke_comment, tke_item_id, tke_edit_rate) VALUES ($nextId, $x->{'TAKENO'}, $x->{'LOCATION'}, '$x->{'DATE'}', $x->{'START'}, $x->{'LENGTH'}, $x->{'RESULT'}, '$x->{'COMMENT'}', $x->{'ITEM'}, ($x->{'EDITRATE'}, $x->{'EDITRATEDENOM'}));";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $nextId;
}

sub update_take
{
    my ($dbh, $x) = @_;

    eval
    {
        
        my $sth = $dbh->prepare("
            UPDATE Take
			SET
				tke_number = ?,
				tke_recording_location = ?,
				tke_start_date = ?,
				tke_start_position = ?,
				tke_length = ?,
				tke_result = ?,
				tke_comment = ?,
				tke_edit_rate = ?,
				tke_item_id = ?
            WHERE
				tke_identifier = ?
            ");
        $sth->execute($x->{"TAKENO"}, $x->{"LOCATIONID"}, $x->{"DATE"}, $x->{"START"}, $x->{"LENGTH"}, $x->{"RESULTID"}, $x->{"COMMENT"}, $x->{"EDITRATE"}, $x->{"ITEM"}, $x->{"ID"});
        
        $dbh->commit;
    };
    if ($@)
    {
        $errstr = (defined $dbh->errstr) ? $dbh->errstr : "unknown error";
        eval { $dbh->rollback; };
        return undef;
    }
    
    return $x->{"ID"};
}

1;

