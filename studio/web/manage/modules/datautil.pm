#
# $Id: datautil.pm,v 1.3 2009/01/29 07:36:59 stuart_hc Exp $
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
 
package datautil;

use strict;
use warnings;


####################################
#
# Module exports
#
####################################

BEGIN 
{
    use Exporter ();
    our (@ISA, @EXPORT);

    @ISA = qw(Exporter);
    @EXPORT = qw(
        &create_source_config
        &create_recorder
        &create_recorder_config
        &create_multicam_config
        &create_proxy_def
        &create_router_config
    );
}



####################################
#
# Exported package globals
#
####################################



####################################
#
# Source config
#
####################################

sub create_source_config
{
    my ($typeid, $name, $locationodOrSpoolid, $numVideo, $numAudio) = @_;
    
    my $scf;
    
    if ($typeid == $prodautodb::sourceType{"Tape"})
    {
        $scf = {
            config => 
            {
                NAME => $name,
                TYPE_ID => $typeid,
                SPOOL => $locationodOrSpoolid,
                LOCATION_ID => undef,
            }
        };
    }
    else
    {
        $scf = {
            config => 
            {
                NAME => $name,
                TYPE_ID => $typeid,
                SPOOL => undef,
                LOCATION_ID => $locationodOrSpoolid,
            }
        };
    }
    
    foreach (1..$numVideo)
    {
        _add_source_config_track($scf, $prodautodb::dataDef{"Picture"});
    }
    foreach (1..$numAudio)
    {
        _add_source_config_track($scf, $prodautodb::dataDef{"Sound"});
    }

    return $scf;
}

sub _add_source_config_track
{
    my ($scf, $datadefid) = @_;

    my $trackId;
    my $trackNumber;
    
    if (!defined $scf->{"tracks"})
    {
        $trackId = 1;
        $trackNumber = 1;
    }
    else
    {
        my $lastTrack = $scf->{"tracks"}[ $#{ $scf->{"tracks"} } ];

        $trackId = $lastTrack->{"TRACK_ID"} + 1;
        
        # we assume that video tracks are added first followed by audio tracks
        # a change in data def signals the re-start of track number counting 
        if ($datadefid != $lastTrack->{"DATA_DEF_ID"})
        {
            $trackNumber = 1;
        }
        else
        {
            $trackNumber = $lastTrack->{"TRACK_NUMBER"} + 1;
        }
    }

    my $trackName = ($datadefid == $prodautodb::dataDef{"Picture"}) ? "V".$trackNumber : "A".$trackNumber;
    
    push (@{ $scf->{"tracks"} }, {
                TRACK_ID => $trackId,
                TRACK_NUMBER => $trackNumber,
                NAME => $trackName,
                DATA_DEF_ID => $datadefid,
                EDIT_RATE_NUM => 25,
                EDIT_RATE_DEN => 1,
                LENGTH => 10800000,
            });
}



####################################
#
# Recorder
#
####################################

sub create_recorder
{
    my ($name) = @_;
    
    my $rec = {
        NAME => $name,
        CONF_ID => undef,
    };
    
    return $rec;
}


####################################
#
# Recorder config
#
####################################

sub create_recorder_config
{
    my ($recId, $name, $numInputs, $numTracks, $drps) = @_;
    
    my $rcf = {
        config => 
        {
            NAME => $name,
            RECORDER_ID => $recId,
        },
        parameters => $drps,
    };
    
    foreach my $inputIndex (1..$numInputs)
    {
        _add_recorder_config_input($rcf, $inputIndex, $numTracks);
    }

    return $rcf;
}

sub _add_recorder_config_input
{
    my ($rcf, $index, $numTracks) = @_;

    if (!defined $rcf->{"inputs"})
    {
        $rcf->{"inputs"} = ();
    }
    
    my @tracks;        
    foreach my $trackIndex (1..$numTracks)
    {
        push(@tracks, {
            INDEX => $trackIndex,
            TRACK_NUMBER => 0,
            SOURCE_ID => undef,
            SOURCE_TRACK_ID => undef,
        });
    }
    
    push (@{ $rcf->{"inputs"} }, {
        config => {
            INDEX => $index,
            NAME => "Input $index",
            },
        tracks => \@tracks,
        });

}


####################################
#
# Multi-camera config
#
####################################

sub create_multicam_config
{
    my ($name, $numVideoSelect, $numAudio, $numAudioSelect) = @_;

    my $mccf = {
        config => 
        {
            NAME => $name,
        }
    };
    
    # video track with multiple source selector
    _add_multicam_track($mccf, 1, $numVideoSelect);
    
    # audio tracks with multiple source selector
    foreach my $trackIndex (2..($numAudio + 1))
    {
        _add_multicam_track($mccf, $trackIndex, $numAudioSelect);
    }

    return $mccf;
}

sub _add_multicam_track
{
    my ($mccf, $trackIndex, $numSelect) = @_;

    if (!defined $mccf->{"tracks"})
    {
        $mccf->{"tracks"} = ();
    }
    
    my @selectors;        
    foreach my $selectorIndex (1..$numSelect)
    {
        push(@selectors, {
            INDEX => $selectorIndex,
            SOURCE_ID => undef,
            SOURCE_TRACK_ID => undef,
        });
    }
    
    push (@{ $mccf->{"tracks"} }, {
        config => {
            INDEX => $trackIndex,
            TRACK_NUMBER => 0,
        },
        selectors => \@selectors,
    });

}


####################################
#
# Proxy definition
#
####################################

sub create_proxy_def
{
    my ($name, $typeID) = @_;

    my $pxdf = {
        def => 
        {
            NAME => $name,
            TYPE_ID => $typeID,
        }
    };
    
    if ($typeID == 1)
    {
        # simple web
        
        # video track
        _add_proxy_track($pxdf, 1, 1, 1);
        
        # stereo audio track
        _add_proxy_track($pxdf, 2, 2, 2);
    }
    elsif ($typeID == 2)
    {
        # quad split

        # quad split video track
        _add_proxy_track($pxdf, 1, 1, 4);
        
        # stereo audio track
        _add_proxy_track($pxdf, 2, 2, 2);
    }
    else
    {
        return undef;
    }
    
    return $pxdf;
}

sub _add_proxy_track
{
    my ($pxdf, $trackID, $dataDef, $numSources) = @_;

    if (!defined $pxdf->{"tracks"})
    {
        $pxdf->{"tracks"} = ();
    }
    
    my @sources;        
    foreach my $sourcesIndex (1..$numSources)
    {
        push(@sources, {
            INDEX => $sourcesIndex,
            SOURCE_ID => undef,
            SOURCE_TRACK_ID => undef,
        });
    }
    
    push (@{ $pxdf->{"tracks"} }, {
        def => {
            TRACK_ID => $trackID,
            DATA_DEF_ID => $dataDef,
        },
        sources => \@sources,
    });

}


####################################
#
# Router config
#
####################################

sub create_router_config
{
    my ($name, $numInputs, $numOutputs) = @_;
    
    my $rocf = {
        config => 
        {
            NAME => $name,
        },
    };
    
    foreach my $inputIndex (1..$numInputs)
    {
        _add_router_config_input($rocf, $inputIndex);
    }

    foreach my $outputIndex (1..$numOutputs)
    {
        _add_router_config_output($rocf, $outputIndex);
    }

    return $rocf;
}

sub _add_router_config_input
{
    my ($rocf, $index) = @_;

    if (!defined $rocf->{"inputs"})
    {
        $rocf->{"inputs"} = ();
    }
    
    push (@{ $rocf->{"inputs"} }, {
            INDEX => $index,
            NAME => "Input $index",
            SOURCE_ID => undef,
            SOURCE_TRACK_ID => undef,
        });
}

sub _add_router_config_output
{
    my ($rocf, $index) = @_;

    if (!defined $rocf->{"outputs"})
    {
        $rocf->{"outputs"} = ();
    }
    
    push (@{ $rocf->{"outputs"} }, {
            INDEX => $index,
            NAME => "Output $index",
        });
}





1;

