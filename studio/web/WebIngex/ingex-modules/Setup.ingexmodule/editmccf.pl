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


if (!defined param("id"))
{
    return_error_page("Missing 'id' parameter");
}

if (defined param("Cancel"))
{
    redirect_to_page("multicamcf.pl");
}


my $mccfId = param("id");
my $errorMessage;

my $mccf = load_multicam_config($dbh, $mccfId) 
    or return_error_page("failed to find multi-camera config with id=$mccfId from database: $prodautodb::errstr");

 
if (defined param("Reset"))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($mccf)))
    {
        $mccf->{"config"}->{"NAME"} = param("name");
        
        foreach my $track (@{ $mccf->{"tracks"} })
        {
            my $trackNumId = get_html_param_id([ "track", "num"  ], [ $track->{"config"}->{"ID"} ]);
            $track->{"config"}->{"TRACK_NUMBER"} = param($trackNumId);
            
            foreach my $selector (@{ $track->{"selectors"} })
            {
                my $selectorSourceId = get_html_param_id([ "track", "selector", "source" ], 
                    [ $track->{"config"}->{"ID"}, $selector->{"ID"} ]
                );
                my @sourceTrack = parse_html_source_track(param($selectorSourceId));
                $selector->{"SOURCE_ID"} = $sourceTrack[0];
                $selector->{"SOURCE_TRACK_ID"} = $sourceTrack[1];
            }
        }            
 
        if (!update_multicam_config($dbh, $mccf))
        {
            # do same as a reset
            Delete_all();
            $mccf = load_multicam_config($dbh, $mccfId) or
                return_error_page("failed to find multi-camera config with id=$mccfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update multi-camera config to database: $prodautodb::errstr";
        }
        else
        {
            $mccf = load_multicam_config($dbh, $mccfId) or
                return_error_page("failed to reload multi-camera config from database: $prodautodb::errstr");
    
            redirect_to_page("multicamcf.pl");
        }
        
    }
}


return_edit_page($mccf, $errorMessage);





sub validate_params
{
    my ($mccf) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    foreach my $track (@{ $mccf->{"tracks"} })
    {
        my $trackNumId = get_html_param_id([ "track", "num"  ], [ $track->{"config"}->{"ID"} ]);
        return "Error: missing or invalid track number for track $track->{'config'}->{'INDEX'}" 
            if (!defined param($trackNumId) || 
                param($trackNumId) && param($trackNumId) !~ /^\d+$/);

        foreach my $selector (@{ $track->{"selectors"} })
        {
            my $selectorSourceId = get_html_param_id([ "track", "selector", "source" ], 
                [ $track->{"config"}->{"ID"}, $selector->{"ID"} ]);
            return "Error: missing selector source for track $track->{'config'}->{'INDEX'}, selector $selector->{'INDEX'}" 
                if (!defined param($selectorSourceId));
    
            my $selectorSourceIdParam = param($selectorSourceId);
            my @sourceTrack = parse_html_source_track($selectorSourceIdParam);
            return "Error: invalid selector source reference ('$selectorSourceIdParam') for track $track->{'config'}->{'INDEX'}, selector $selector->{'INDEX'}"
                if (scalar @sourceTrack != 2 || 
                    (defined $sourceTrack[0] && $sourceTrack[0] !~ /^\d+$/) || 
                    (defined $sourceTrack[1] && $sourceTrack[1] !~ /^\d+$/));
        }
            
    }
    
    return undef;
}

sub return_edit_page
{
    my ($mccf, $errorMessage) = @_;

    my $page = get_edit_content($mccf, $errorMessage) or
        return_error_page("failed to fill in content for edit multi-camera config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($mccf, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Edit multi-camera clip definition"));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editmccf')"}));

    push(@pageContent, hidden("id", $mccf->{"config"}->{"ID"}));


    my @trackRows;
    push(@trackRows,
        Tr({-align=>"left", -valign=>"top"}, [
           th(["Index", "Clip track num", "Selectors"]),
        ]));
        
    foreach my $track (@{ $mccf->{"tracks"} })
    {
        my @selectorRows;
        push(@selectorRows, 
            Tr({-align=>"left", -valign=>"top"}, [
                th(["Index", "Source"]),
            ]));

        foreach my $selector (@{ $track->{"selectors"} })
        {
            push(@selectorRows, 
                Tr({-align=>"left", -valign=>"top"}, [
                    td([div($selector->{"INDEX"}, 
                            hidden(
                                get_html_param_id([ "track", "selector", "index" ], [ $track->{"config"}->{"ID"}, $selector->{"ID"} ]),
                                $selector->{"INDEX"}
                            )
                        ),
                        get_sources_popup(
                            get_html_param_id([ "track", "selector", "source" ], [ $track->{"config"}->{"ID"}, $selector->{"ID"} ]),
                            load_source_config_refs($dbh),
                            $selector->{"SOURCE_ID"}, 
                            $selector->{"SOURCE_TRACK_ID"}
                        ),
                    ]),
                ])
            );
        }


        push(@trackRows, 
            Tr({-align=>"left", -valign=>"top"}, [
                td([div($track->{"config"}->{"INDEX"},
                        hidden(
                            get_html_param_id([ "track", "index" ], [ $track->{"config"}->{"ID"} ]), 
                            $track->{"config"}->{"INDEX"}
                        )
                    ),
                    textfield(
                        get_html_param_id([ "track", "num" ], [ $track->{"config"}->{"ID"} ]),
                        $track->{"config"}->{"TRACK_NUMBER"}
                    ),
                    table({-class=>"borderTable"}, 
                        @selectorRows
                    ),
                ]),
            ])
        );
    }

    
    my @topRows;

    push(@topRows,  
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), 
                textfield("name", $mccf->{"config"}->{"NAME"})]
            ),
        ]),
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Tracks:"), 
                table({-class=>"borderTable"}, @trackRows),
            ]),
        ]),
    );
    

    push(@pageContent, table({-class=>"noBorderTable"}, @topRows));
    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"})
        )
    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


