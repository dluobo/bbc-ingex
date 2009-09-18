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
    redirect_to_page("recorder.pl");
}


my $recId = param("id");
my $errorMessage;

my $rec = load_recorder($dbh, $recId) or
    return_error_page("failed to find recorder with id=$recId from database: $prodautodb::errstr");

my $rcfs = load_recorder_configs($dbh, $recId) or
    return_error_page("failed to load recorder configs for recorder id=$recId from database: $prodautodb::errstr");

 
if (defined param("Reset"))
{
    Delete_all();
}
elsif (defined param("Done"))
{
    if (!($errorMessage = validate_params($rcfs)))
    {
        $rec->{"NAME"} = param("name");
        
        if (param("confid"))
        {
            $rec->{"CONF_ID"} = param("confid");
        }
        else
        {
            $rec->{"CONF_ID"} = undef;
        }
 
        if (!update_recorder($dbh, $rec))
        {
            # do same as a reset
            Delete_all();
            $rec = load_recorder($dbh, $recId) or
                return_error_page("failed to find recorder with id=$recId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update recorder to database: $prodautodb::errstr";
        }
        else
        {
            $rec = load_recorder($dbh, $recId) or
                return_error_page("failed to reload saved recorder from database: $prodautodb::errstr");
    
            redirect_to_page("recorder.pl");
        }
        
    }
}


return_edit_page($rec, $rcfs, $errorMessage);





sub validate_params
{
    my ($rcfs) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);

    if (param("confid"))
    {
        my $foundConfig = 0;    
        foreach my $rcf (@{ $rcfs })
        {
            if ($rcf->{"config"}->{"ID"} == param("confid"))
            {
                $foundConfig = 1;
                last;
            }
        }
        
        return "Error: recorder config with id=" . param("confid") . "is not associated with recorder" 
            if (!$foundConfig);
    }
    
    
    return undef;
}

sub return_edit_page
{
    my ($rec, $rcfs, $errorMessage) = @_;

    my $page = get_edit_content($rec, $rcfs, $errorMessage) or
        return_error_page("failed to fill in content for edit recorder config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($rec, $rcfs, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Edit recorder"));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editrec')"}));

    push(@pageContent, hidden("id", $rec->{"ID"}));

    my @topRows;

    push(@topRows,  
        Tr({-class=>"simpleTable", -align=>"left", -valign=>"top"}, [
            td([div({-class=>"propHeading1"}, "Name:"), 
                textfield("name", $rec->{"NAME"})
            ]),
            td([div({-class=>"propHeading1"}, "Config:"), 
                get_recorder_config_popup("confid", $rcfs, $rec->{"CONF_ID"})
            ]),
            
        ])
    );

    push(@pageContent, table({-class=>"noBorderTable"},
        @topRows));

        
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


