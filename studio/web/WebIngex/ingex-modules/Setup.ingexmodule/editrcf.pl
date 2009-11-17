#!/usr/bin/perl -wT

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
 
use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use lib "../../ingex-config";
use ingexconfig;
use prodautodb;
use db;
use datautil;
use htmlutil; use ingexhtmlutil;;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();


if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}


my $rcfId = param('id');
my $errorMessage;

my $vrs = load_video_resolutions($dbh) 
    or return_error_page("failed to load video resolutions: $prodautodb::errstr");

my $fmts = load_file_formats($dbh)
	or return_error_page("failed to load file formats: $prodautodb::errstr");

my $rcf = load_recorder_config($dbh, $rcfId) or
    return_error_page("failed to find recorder config with id=$rcfId from database: $prodautodb::errstr");

 
if (defined param('Reset'))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($rcf)))
    {
        $rcf->{"config"}->{"NAME"} = param("name");
        
        foreach my $recParam (@{ $rcf->{"parameters"} })
        {
            $recParam->{"VALUE"} = param("param-$recParam->{'ID'}");
        }
        
        foreach my $input (@{ $rcf->{"inputs"} })
        {
            my $inputNameId = get_html_param_id([ "input", "name"  ], [ $input->{"config"}->{"ID"} ]);
            $input->{"config"}->{"NAME"} = param($inputNameId);
            
            foreach my $track (@{ $input->{"tracks"} })
            {
                my $trackNumId = get_html_param_id([ "input", "track", "num" ], [ $input->{"config"}->{"ID"}, $track->{"ID"} ]);
                $track->{"TRACK_NUMBER"} = param($trackNumId);

                my $trackSourceId = get_html_param_id([ "input", "track", "source" ], [ $input->{"config"}->{"ID"}, $track->{"ID"} ]);
                my @sourceTrack = parse_html_source_track(param($trackSourceId));
                $track->{"SOURCE_ID"} = $sourceTrack[0];
                $track->{"SOURCE_TRACK_ID"} = $sourceTrack[1];
            }
        }            
 
        if (!update_recorder_config($dbh, $rcf))
        {
            # do same as a reset
            Delete_all();
            $rcf = load_recorder_config($dbh, $rcfId) or
                return_error_page("failed to find recorder config with id=$rcfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update recorder config to database: $prodautodb::errstr";
        }
        else
        {
            $rcf = load_recorder_config($dbh, $rcfId) or
                return_error_page("failed to reload saved recorder config from database: $prodautodb::errstr");
    
            redirect_to_page("recorder.pl");
        }
        
    }
}


return_edit_page($rcf, $vrs, $fmts, $errorMessage);





sub validate_params
{
    my ($rcf) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);

    foreach my $recParam (@{ $rcf->{"parameters"} })
    {
        return "Error: missing recorder parameter $recParam->{'NAME'}" 
            if (!defined param("param-$recParam->{'ID'}"));
    }
    
    foreach my $input (@{ $rcf->{"inputs"} })
    {
        my $inputNameId = get_html_param_id([ "input", "name"  ], [ $input->{"config"}->{"ID"} ]);
        return "Error: empty name for input $input->{'config'}->{'INDEX'}" if (!defined param($inputNameId) ||
            param($inputNameId) =~ /^\s*$/);
            
        foreach my $track (@{ $input->{"tracks"} })
        {
            my $trackNumId = get_html_param_id([ "input", "track", "num"  ], [ $input->{"config"}->{"ID"}, $track->{"ID"} ]);
            return "Error: missing or invalid track number for input $input->{'config'}->{'INDEX'}, track $track->{'INDEX'}" 
                if (!defined param($trackNumId) || 
                    param($trackNumId) && param($trackNumId) !~ /^\d+$/);
    
            my $trackSourceId = get_html_param_id([ "input", "track", "source"  ], [ $input->{"config"}->{"ID"}, $track->{"ID"} ]);
            return "Error: missing track source for input $input->{'config'}->{'INDEX'}, track $track->{'INDEX'}" 
                if (!defined param($trackSourceId));
    
            my $trackSourceIdParam = param($trackSourceId);
            my @sourceTrack = parse_html_source_track($trackSourceIdParam);
            return "Error: invalid track source reference ('$trackSourceIdParam') for input $input->{'config'}->{'INDEX'}, track $track->{'INDEX'}"
                if (scalar @sourceTrack != 2 || 
                    (defined $sourceTrack[0] && $sourceTrack[0] !~ /^\d+$/) || 
                    (defined $sourceTrack[1] && $sourceTrack[1] !~ /^\d+$/));
        }
            
    }
    
    
    return undef;
}

sub return_edit_page
{
    my ($rcf, $vrs, $fmts, $errorMessage) = @_;

    my $page = get_edit_content($rcf, $vrs, $fmts, $errorMessage) or
        return_error_page("failed to fill in content for edit recorder config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($rcf, $vrs, $fmts, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit recorder configuration'));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editrcf')"}));

    push(@pageContent, hidden('id', $rcf->{'config'}->{'ID'}));

    my @topRows;

    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([
                div({-class=>"propHeading1"}, 'Recorder:'), 
                $rcf->{'config'}->{'RECORDER_NAME'}
            ]),
        ]),
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([
                div({-class=>"propHeading1"}, 'Name:'), 
                textfield('name', $rcf->{'config'}->{'NAME'})
            ]),
        ])
    );

    my @inputRows;
    foreach my $input (@{ $rcf->{'inputs'} })
    {
        push(@inputRows, 
            Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
                td([div({-class=>"propHeading2"}, 'Index:'), 
                    div($input->{'config'}->{'INDEX'},
                        hidden(
                            get_html_param_id([ "input", "index" ], [ $input->{'config'}->{'ID'} ]), 
                            $input->{'config'}->{'INDEX'}
                        ),
                    ),
                 ]),
            ]),
            Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
                td([div({-class=>"propHeading2"}, 'Name:'), 
                    textfield(
                        get_html_param_id([ "input", "name" ], [ $input->{'config'}->{'ID'} ]),
                        $input->{'config'}->{'NAME'}
                    ),
                ]),
            ]),
        );

        my @trackRows;
        push(@trackRows, 
            Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
                th(['Index', 'Source', 'Clip Track Number']),
            ])
        );

        foreach my $track (@{ $input->{'tracks'} })
        {
            push(@trackRows, 
                Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
                    td([div($track->{'INDEX'}, 
                            hidden(
                                get_html_param_id([ "input", "track", "index" ], [ $input->{'config'}->{'ID'}, $track->{'ID'} ]),
                                $track->{'INDEX'}
                            )
                        ),
                        get_sources_popup(
                            get_html_param_id([ "input", "track", "source" ], [ $input->{'config'}->{'ID'}, $track->{'ID'} ]),
                            load_source_config_refs($dbh),
                            $track->{'SOURCE_ID'}, 
                            $track->{'SOURCE_TRACK_ID'},
                        ),
                        textfield(
                            get_html_param_id([ "input", "track", "num" ], [ $input->{'config'}->{'ID'}, $track->{'ID'} ]),
                            $track->{'TRACK_NUMBER'}
                        ),
                    ]),
                ]),
            );
        }

        push(@inputRows,  
            Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
                td([div({-class=>"propHeading2"}, 'Tracks:'), 
                    table({-class=>"borderTable"}, @trackRows),
                ]),
            ]),
        );
    }

    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Inputs:'), 
                table({-class=>"noBorderTable"}, @inputRows),
            ]),
        ]),
    );

    my @paramRows;
    push(@paramRows,
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
           th(["Name", "Value"]),
        )
    );
    foreach my $rp (sort { $a->{"NAME"} cmp $b->{"NAME"} } (@{ $rcf->{"parameters"} }))
    {
        my $valueField;
        if ($rp->{"NAME"} eq "MXF_RESOLUTION" ||
            $rp->{"NAME"} =~ /ENCODE(\d*)_RESOLUTION/ ||
            $rp->{"NAME"} eq "QUAD_RESOLUTION")
        {
            $valueField = get_video_resolution_popup("param-$rp->{'ID'}",
                $vrs, $rp->{"VALUE"});
        }
        elsif ($rp->{"NAME"} eq "MXF_WRAPPING" ||
            $rp->{"NAME"} =~ /ENCODE(\d*)_WRAPPING/ ||
            $rp->{"NAME"} eq "QUAD_WRAPPING")
        {
        	$valueField = get_video_wrapping_popup("param-$rp->{'ID'}",
                $fmts, $rp->{"VALUE"});
        }
        else
        {
            $valueField = textfield(
                -name => "param-$rp->{'ID'}", 
                -value => $rp->{"VALUE"},
                -size => 20,
                -maxlength => 250,
            );
        }
        
        push(@paramRows,
            Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, 
                td([$rp->{"NAME"}, $valueField]),
            )
        );
    }
    
    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, "Parameters:"), 
                table({-class=>"borderTable"}, @paramRows),
            ]),
        ]),
    );

    
    push(@pageContent, table({-class=>"noBorderTable"}, @topRows));

    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        )
    );

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


