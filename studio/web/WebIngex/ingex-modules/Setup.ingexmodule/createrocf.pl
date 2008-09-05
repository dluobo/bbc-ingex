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
    redirect_to_page("router.pl");
}


my $rocfs = load_router_configs($dbh) 
    or return_error_page("failed to load router configs: $prodautodb::errstr");


my $errorMessage;

if (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $rocf = datautil::_load_next_id(trim(param("name")), param("numinputs"), param("numoutputs"));

        my $rocfId = save_router_config($dbh, $rocf) 
            or return_create_page("failed to save router config to database: $prodautodb::errstr", $rocf);
        
        redirect_to_page("editrocf.pl?id=$rocfId");
    }
}

return_create_page($errorMessage, $rocfs);



sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Invalid number of inputs" if (!param("numinputs") || 
        param("numinputs") !~ /^\d+$/);
        
    return "Error: Invalid number of outputs" if (!param("numoutputs") || 
        param("numoutputs") !~ /^\d+$/);
        
    return undef;
}

sub return_create_page
{
    my ($errorMessage, $rocfs) = @_;
    
    my $page = get_create_content($errorMessage, $rocfs) or
        return_error_page("failed to fill in content for create router config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_content
{
    my ($message, $rocfs) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new router configuration"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','createrocf')"}));

    
    my @tableRows;
    
    push(@tableRows,  
        Tr({-align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), textfield("name")]),
            td([div({-class=>"propHeading1"}, "# Inputs:"), 
                textfield(
                    -name => "numinputs", 
                    -default => 1 
                )
            ]),
            td([div({-class=>"propHeading1"}, "# Outputs:"), 
                textfield(
                    -name => "numoutputs", 
                    -default => 1 
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


