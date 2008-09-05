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
    redirect_to_page("proxydf.pl");
}


my $pxdfId = param("id");
my $errorMessage;

my $pxdf = load_proxy_def($dbh, $pxdfId) 
    or return_error_page("failed to find proxy definition with id=$pxdfId from database: $prodautodb::errstr");

 
if (defined param("Reset"))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($pxdf)))
    {
        $pxdf->{"def"}->{"NAME"} = param("name");
        
        foreach my $track (@{ $pxdf->{"tracks"} })
        {
            foreach my $source (@{ $track->{"sources"} })
            {
                my $sourceId = get_html_param_id([ "track", "source", "source" ], 
                    [ $track->{"def"}->{"ID"}, $source->{"ID"} ]
                );
                my @sourceTrack = parse_html_source_track(param($sourceId));
                $source->{"SOURCE_ID"} = $sourceTrack[0];
                $source->{"SOURCE_TRACK_ID"} = $sourceTrack[1];
            }
        }            
 
        if (!update_proxy_def($dbh, $pxdf))
        {
            # do same as a reset
            Delete_all();
            $pxdf = load_proxy_def($dbh, $pxdfId) or
                return_error_page("failed to find proxy definition with id=$pxdfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update proxy definition to database: $prodautodb::errstr";
        }
        else
        {
            $pxdf = load_proxy_def($dbh, $pxdfId) or
                return_error_page("failed to reload proxy definition from database: $prodautodb::errstr");
    
            redirect_to_page("proxydf.pl");
        }
        
    }
}


return_edit_page($pxdf, $errorMessage);





sub validate_params
{
    my ($pxdf) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    foreach my $track (@{ $pxdf->{"tracks"} })
    {
        foreach my $source (@{ $track->{"sources"} })
        {
            my $sourceId = get_html_param_id([ "track", "source", "source" ], 
                [ $track->{"def"}->{"ID"}, $source->{"ID"} ]);
            return "Error: missing source source for track $track->{'def'}->{'TRACK_ID'}, source $source->{'INDEX'}" 
                if (!defined param($sourceId));
    
            my $sourceIdParam = param($sourceId);
            my @sourceTrack = parse_html_source_track($sourceIdParam);
            return "Error: invalid source source reference ('$sourceIdParam') for track $track->{'def'}->{'TRACK_ID'}, source $source->{'INDEX'}"
                if (scalar @sourceTrack != 2 || 
                    (defined $sourceTrack[0] && $sourceTrack[0] !~ /^\d+$/) || 
                    (defined $sourceTrack[1] && $sourceTrack[1] !~ /^\d+$/));
        }
            
    }
    
    return undef;
}

sub return_edit_page
{
    my ($pxdf, $errorMessage) = @_;

    my $page = get_edit_content($pxdf, $errorMessage) or
        return_error_page("failed to fill in content for edit proxy definition page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($pxdf, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Edit proxy definition"));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editproxydf')"}));

    push(@pageContent, hidden("id", $pxdf->{"def"}->{"ID"}));


    my @trackRows;
    push(@trackRows,
        Tr({-align=>"left", -valign=>"top"}, [
           th(["ID", "Data def", "Sources"]),
        ]));
        
    foreach my $track (@{ $pxdf->{"tracks"} })
    {
        my @sourceRows;
        push(@sourceRows, 
            Tr({-align=>"left", -valign=>"top"}, [
                th(["Index", "Source"]),
            ]));

        foreach my $source (@{ $track->{"sources"} })
        {
            push(@sourceRows, 
                Tr({-align=>"left", -valign=>"top"}, [
                    td([div($source->{"INDEX"}), 
                        get_sources_popup(
                            get_html_param_id([ "track", "source", "source" ], [ $track->{"def"}->{"ID"}, $source->{"ID"} ]),
                            load_source_config_refs($dbh),
                            $source->{"SOURCE_ID"}, 
                            $source->{"SOURCE_TRACK_ID"}
                        ),
                    ]),
                ])
            );
        }


        push(@trackRows, 
            Tr({-align=>"left", -valign=>"top"}, [
                td([$track->{"def"}->{"TRACK_ID"},
                    $track->{"def"}->{"DATA_DEF"},
                    table({-class=>"borderTable"}, @sourceRows),
                ]),
            ])
        );
    }

    
    my @topRows;

    push(@topRows,  
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), 
                textfield("name", $pxdf->{"def"}->{"NAME"})]
            ),
        ]),
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Type:"), 
                $pxdf->{"def"}->{"TYPE"}
            ]),
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


