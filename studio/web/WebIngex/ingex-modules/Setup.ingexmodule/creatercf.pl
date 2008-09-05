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


if (defined param("Cancel"))
{
    redirect_to_page("recorder.pl");
}
if (!defined param("recid"))
{
    return_error_page("Missing 'recid' parameter");
}


my $rec = load_recorder($dbh, param("recid")) 
    or return_error_page("failed to load recorder: $prodautodb::errstr");

my $rcfs = load_recorder_configs($dbh, param("recid")) 
    or return_error_page("failed to load recorder configs for recorder: $prodautodb::errstr");

my $drps = load_default_recorder_parameters($dbh) 
    or return_error_page("failed to load default recorder parameters: $prodautodb::errstr");
    
    
my $errorMessage;

if (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $rcf = create_recorder_config(param("recid"), trim(param("name")), 
            param("numinputs"), param("numtracks"), $drps);

        my $rcfId = save_recorder_config($dbh, $rcf) 
            or return_create_page("failed to save recorder config to database: $prodautodb::errstr", $rec);
        
        if (!$rec->{"CONF_ID"})
        {
            $rec->{"CONF_ID"} = $rcfId;
            if (!update_recorder($dbh, $rec))
            {
                return_create_page("failed to update recorder with new config: $prodautodb::errstr", $rec);
            }
        }
            
        redirect_to_page("editrcf.pl?id=$rcfId");
    }
}

return_create_page($errorMessage, $rec, $rcfs);



sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Invalid number of inputs" if (!param("numinputs") || 
        param("numinputs") !~ /^\d+$/);
        
    return "Error: Invalid number of tracks per input" if (!param("numtracks") || 
        param("numtracks") !~ /^\d+$/);

    return undef;
}

sub return_create_page
{
    my ($errorMessage, $rec, $rcfs) = @_;
    
    my $page = get_create_content($errorMessage, $rec, $rcfs) or
        return_error_page("failed to fill in content for create recorder config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_num_tracks
{
    my ($rcfs) = @_;

    # use previous    
    if (defined param("numtracks"))
    {
        return param("numtracks");
    }
    
    # use value in an existing input track    
    foreach my $rcf (@{ $rcfs } )
    {
        foreach my $rcft (@{ $rcf->{"inputs"} } )
        {
            return scalar @{ $rcft->{"tracks"} };
        }
    }
    
    # use default    
    return 5;
}

sub get_num_inputs
{
    my ($rcfs) = @_;

    # use previous    
    if (defined param("numinputs"))
    {
        return param("numinputs");
    }
    
    # use value in an existing input    
    foreach my $rcf (@{ $rcfs } )
    {
        return scalar @{ $rcf->{"inputs"} };
    }
    
    # use default    
    return 1;
}

sub get_create_content
{
    my ($message, $rec, $rcfs) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new recorder configuration"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','creatercf')"}));

    
    push(@pageContent, hidden(-name=>"recid", -value=>$rec->{"ID"}));

    my @tableRows;
    
    push(@tableRows,  
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Recorder:"), $rec->{"NAME"}]),
            td([div({-class=>"propHeading1"}, "Name:"), textfield("name")]),
            td([div({-class=>"propHeading1"}, "# Inputs:"), 
                textfield(
                    -name => "numinputs", 
                    -default => get_num_inputs($rcfs), 
                    -override => 1
                )
            ]),
            td([div({-class=>"propHeading1"}, "# Tracks per inputs:"), 
                textfield(
                    -name => "numtracks", 
                    -default => get_num_tracks($rcfs),
                    -override => 1
                )
            ]),
        ])
    );


    push(@pageContent, table({-class=>"noBorderTable"}, @tableRows));
    
    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Create"}), 
            span(" "), submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"})
        )
    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


