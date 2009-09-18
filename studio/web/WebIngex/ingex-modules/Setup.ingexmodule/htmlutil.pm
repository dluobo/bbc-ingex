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
 
package htmlutil;

use strict;
use warnings;

use CGI::Pretty qw(:standard);

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
        &get_html_source_track
        &parse_html_source_track
        &get_sources_popup
        &get_source_config
        &get_video_resolution_popup
        &get_recorder_config_popup
        &get_recorder_config
        &get_proxy_def
        &get_proxy_types_popup
        &get_router_config
		&get_node
    );
}

####################################
#
# Source config
#
####################################

sub get_html_source_track
{
    my ($sourceId, $trackId) = @_;

    $sourceId = ($sourceId) ? $sourceId : 0;
    $trackId = ($trackId) ? $trackId : 0;
    
    return "$sourceId $trackId";
}

sub parse_html_source_track
{
    my ($htmlValue) = @_;
    
    my @sourceTrack = split(" ", $htmlValue);
    if (!$sourceTrack[0])
    {
        $sourceTrack[0] = undef;
    }
    if (!$sourceTrack[1])
    {
        $sourceTrack[1] = undef;
    }
    
    return @sourceTrack;
}

sub get_sources_popup
{
    my ($paramId, $scfrs, $defaultSourceId, $defaultSourceTrackId) = @_;
    
    my $default;
    my @values;
    my %labels;

    # not connected option
    push(@values, get_html_source_track(undef, undef));
    $labels{get_html_source_track(undef, undef)} = "not connected";

    # sources options
    foreach my $scfr (@{ $scfrs })
    {
        my $value = get_html_source_track($scfr->{"ID"}, $scfr->{"TRACK_ID"});
        my $label = "$scfr->{'NAME'} ($scfr->{'TRACK_NAME'})";
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default 
            && defined $defaultSourceId && $scfr->{"ID"} == $defaultSourceId 
            && defined $defaultSourceTrackId && $scfr->{"TRACK_ID"} == $defaultSourceTrackId)
        {
            $default = $value;
        }
    }
    
    # no default then is not connected
    if (!defined $default)
    {
        $default = "0 0";
    }
    
    
    return popup_menu(
        -name => $paramId, 
        -default => $default,
        -values => \@values,
        -labels => \%labels);
}

sub get_source_config
{
    my ($sourceConfig) = @_;
    
    my $scfConfig = $sourceConfig->{"config"};
    
    my @pageContent;

    my @tableRows;
    
    push(@tableRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Name:"), $scfConfig->{"NAME"}]),
        ]));
        
    push(@tableRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Type:"), $scfConfig->{"TYPE"}]),
        ]));
    if ($scfConfig->{"TYPE"} eq "LiveRecording")
    {
        push(@tableRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
               td([div({-class=>"propHeading1"}, "Location:"), $scfConfig->{"LOCATION"}]),
            ]));
    }
    else
    {
        push(@tableRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
               td([div({-class=>"propHeading1"}, "Spool number:"), $scfConfig->{"SPOOL"}]),
            ]));
    }

    
    my @trackRows;
    push(@trackRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           th(["Id", "Name", "Type"]),
        ]));
        
    
    foreach my $track (@{$sourceConfig->{"tracks"}})
    {
        push(@trackRows, 
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                td([$track->{"TRACK_ID"}, 
                    $track->{"NAME"},
                    $track->{"DATA_DEF"},]),
            ]));
        
    }
    
    push(@tableRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
            table({-class=>"borderTable"}, @trackRows)]),
        ]));

    
    push(@pageContent, table({-class=>"noBorderTable"},
        @tableRows));

    return join("", @pageContent);
}

####################################
#
# Recorder config
#
####################################

sub get_recorder_config_popup
{
    my ($paramId, $rcfs, $defaultId) = @_;
    
    my $default;
    my @values;
    my %labels;

    # not set option
    push(@values, 0);
    $labels{0} = "not set";

    # sources options
    foreach my $rcf (@{ $rcfs })
    {
        my $value = $rcf->{"config"}->{"ID"};
        my $label = $rcf->{"config"}->{"NAME"};
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default && 
            defined $defaultId && $rcf->{"config"}->{"ID"} == $defaultId)
        {
            $default = $value;
        }
    }
    
    # no default then is not set
    if (!defined $default)
    {
        $default = 0;
    }
    
    
    return popup_menu(
        -name => $paramId, 
        -default => $default,
        -values => \@values,
        -labels => \%labels);
}

sub get_video_resolution_popup
{
    my ($paramId, $vrs, $defaultId) = @_;
    
    my $default;
    my @values;
    my %labels;

    # not set option
    push(@values, 0);
    $labels{0} = "not set (0)";

    foreach my $vrn (@{ $vrs })
    {
        my $value = "$vrn->{'ID'}";
        my $label = "$vrn->{'NAME'} ($value)";
        push(@values, $value);
        $labels{$value} = $label;

        if (!defined $default && 
            defined $defaultId && "$vrn->{'ID'}" eq $defaultId)
        {
            $default = $value;
        }
    }
    
    # no default then is not set
    if (!defined $default)
    {
        $default = 0;
    }
    
    
    return popup_menu(
        -name => $paramId, 
        -default => $default,
        -values => \@values,
        -labels => \%labels);
}

sub get_recorder_config
{
    my ($rcf, $vrs) = @_;
    
    # stop the warnings about deep recursion
    push(@CGI::Pretty::AS_IS, qw(div table th tr td));

    
    my @pageContent;

    my @inputRows;
    foreach my $ricf (@{ $rcf->{"inputs"} })
    {
        my @trackRows;
        push(@trackRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
               th(["Id", "Source Name", "Source Track Name", "Clip Track Number"]),
            )
        );
        foreach my $ritcf (@{ $ricf->{"tracks"} })
        {
            push(@trackRows, 
                Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                    td([$ritcf->{"INDEX"}, 
                        (defined $ritcf->{"SOURCE_NAME"}) ? $ritcf->{"SOURCE_NAME"} : "",
                        (defined $ritcf->{"SOURCE_TRACK_NAME"}) ? $ritcf->{"SOURCE_TRACK_NAME"} : "",
                        (defined $ritcf->{"TRACK_NUMBER"}) ? $ritcf->{"TRACK_NUMBER"} : "",
                    ]),
                )
            );
        }

        push(@inputRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                td([div({-class=>"propHeading2"}, "Index:"), $ricf->{"config"}->{"INDEX"}]),
                td([div({-class=>"propHeading2"}, "Name:"), $ricf->{"config"}->{"NAME"}]),
                td([div({-class=>"propHeading2"}, "Tracks:"), 
                    table({-class=>"borderTable"}, @trackRows)
                ]),
            ])
        );
    }

    my @paramRows;
    push(@paramRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
           th(["Name", "Value"]),
        )
    );
    foreach my $rp (sort { $a->{"NAME"} cmp $b->{"NAME"} } (@{ $rcf->{"parameters"} }))
    {
        my $value = $rp->{"VALUE"};
        if ($rp->{"NAME"} eq "MXF_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE1_RESOLUTION" ||
            $rp->{"NAME"} eq "ENCODE2_RESOLUTION" ||
            $rp->{"NAME"} eq "QUAD_RESOLUTION")
        {
            if ($rp->{"VALUE"} == 0)
            {
                $value = "not set (0)";
            }
            else
            {
                foreach my $vrn (@{ $vrs })
                {
                    if ($vrn->{"ID"} == $rp->{"VALUE"})
                    {
                        $value = "$vrn->{'NAME'} ($value)";
                        last;
                    }
                }
            }
        }
        push(@paramRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                td([$rp->{"NAME"}, $value]),
            )
        );
    }
    
    my @rcfRows;
    push(@rcfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $rcf->{"config"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Recorder:"), $rcf->{"config"}->{"RECORDER_NAME"}]),
            td([div({-class=>"propHeading1"}, "Inputs:"), 
                table({-class=>"noBorderTable"}, @inputRows)
            ]),
            td([div({-class=>"propHeading1"}, "Parameters:"), 
                table({-class=>"noBorderTable"}, @paramRows)
            ]),
        ])
    );
        
    push(@pageContent, table({-class=>"noBorderTable"}, @rcfRows));
      
        
    return join("", @pageContent);
}

####################################
#
# Multi-camera config
#
####################################

sub get_multicam_config
{
    my ($mccf) = @_;
    
    my @pageContent;

    my @mccfRows;

    push(@mccfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $mccf->{"config"}->{"NAME"}]),
        ]));
        

    my @trackRows;
    push(@trackRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           th(["Index", "Clip track num", "Selectors"]),
        ]));
        
    foreach my $mctcf (@{ $mccf->{"tracks"} })
    {
        my @selectorRows;
        push(@selectorRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
               th(["Index", "Source Name", "Source Track Name"]),
            ]));
            
        foreach my $mcscf (@{$mctcf->{"selectors"}})
        {
            push(@selectorRows, 
                Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                    td([$mcscf->{"INDEX"}, 
                        ($mcscf->{"SOURCE_NAME"}) ? $mcscf->{"SOURCE_NAME"} : "",
                        ($mcscf->{"SOURCE_TRACK_NAME"}) ? $mcscf->{"SOURCE_TRACK_NAME"} : "",
                    ]),
                ]));
        }

        push(@trackRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                td([$mctcf->{"config"}->{"INDEX"},
                    $mctcf->{"config"}->{"TRACK_NUMBER"},
                    table({-class=>"borderTable"}, @selectorRows),
                ]),
            ]));
    }

    push(@mccfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
               table({-class=>"borderTable"}, @trackRows)
           ]),
        ]));
        
    push(@pageContent, table({-class=>"noBorderTable"},
        @mccfRows));
        
        
    return join("", @pageContent);
}

####################################
#
# Proxy definition
#
####################################

sub get_proxy_types_popup
{
    my ($popupName, $pxts) = @_;
    
    my $default;
    my @values;
    my %labels;
    foreach my $pxt ( @{ $pxts } )
    {
        push(@values, $pxt->{"ID"});
        $labels{$pxt->{"ID"}} = $pxt->{"NAME"};
        
        if (!defined $default)
        {
            $default = $pxt->{"ID"};
        }
    }
    
    return popup_menu(
        -name=>$popupName,
        -default=>$default,
        -values=>\@values,
        -labels=>\%labels
    );
}

sub get_proxy_def
{
    my ($pxdf) = @_;
    
    my @pageContent;

    my @pxdfRows;

    push(@pxdfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $pxdf->{"def"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Type:"), $pxdf->{"def"}->{"TYPE"}]),
        ]));
        

    my @trackRows;
    push(@trackRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           th(["ID", "Data def", "Sources"]),
        ]));
        
    foreach my $pxtdf (@{ $pxdf->{"tracks"} })
    {
        my @sourceRows;
        push(@sourceRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
               th(["Index", "Source Name", "Source Track Name"]),
            ]));
            
        foreach my $pxsdf (@{$pxtdf->{"sources"}})
        {
            push(@sourceRows, 
                Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                    td([$pxsdf->{"INDEX"}, 
                        ($pxsdf->{"SOURCE_NAME"}) ? $pxsdf->{"SOURCE_NAME"} : "",
                        ($pxsdf->{"SOURCE_TRACK_NAME"}) ? $pxsdf->{"SOURCE_TRACK_NAME"} : "",
                    ]),
                ]));
        }

        push(@trackRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
                td([$pxtdf->{"def"}->{"TRACK_ID"},
                    $pxtdf->{"def"}->{"DATA_DEF"},
                    table({-class=>"borderTable"}, @sourceRows),
                ]),
            ]));
    }

    push(@pxdfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Tracks:"), 
               table({-class=>"borderTable"}, @trackRows)
           ]),
        ]));
        
    push(@pageContent, table({-class=>"noBorderTable"},
        @pxdfRows));
        
        
    return join("", @pageContent);
}

####################################
#
# Router config
#
####################################

sub get_router_config
{
    my ($rocf) = @_;
    
    # stop the warnings about deep recursion
    push(@CGI::Pretty::AS_IS, qw(div table th tr td));

    
    my @pageContent;

    
    # inputs
    
    my @inputRows;
    push(@inputRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
           th(["Index", "Name", "Source Name", "Source Track Name"]),
        )
    );
    foreach my $input (@{ $rocf->{"inputs"} })
    {
        push(@inputRows, 
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                td([$input->{"INDEX"}, $input->{"NAME"}, 
                    (defined $input->{"SOURCE_NAME"}) ? $input->{"SOURCE_NAME"} : "",
                    (defined $input->{"SOURCE_TRACK_NAME"}) ? $input->{"SOURCE_TRACK_NAME"} : "",
                ]),
            )
        );
    }

    
    # outputs
    
    my @outputRows;
    push(@outputRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
           th(["Index", "Name"]),
        )
    );
    foreach my $output (@{ $rocf->{"outputs"} })
    {
        push(@outputRows, 
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                td([$output->{"INDEX"}, $output->{"NAME"}]),
            )
        );
    }

    
    my @rocfRows;
    push(@rocfRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), $rocf->{"config"}->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Inputs:"), 
                table({-class=>"borderTable"}, @inputRows)
            ]),
            td([div({-class=>"propHeading1"}, "Outputs:"), 
                table({-class=>"borderTable"}, @outputRows)
            ]),
        ])
    );
        
    push(@pageContent, table({-class=>"noBorderTable"}, @rocfRows));
      
        
    return join("", @pageContent);
}

1;
