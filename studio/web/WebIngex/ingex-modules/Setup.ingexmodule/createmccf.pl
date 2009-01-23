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


my $errorMessage;

if (defined param("Cancel"))
{
    redirect_to_page("multicamcf.pl");
}
elsif (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $numAudioTracks = param('numaudiotracks') || 0;
        my $numAudioSelects = 0;
        if ($numAudioTracks > 0)
        {
            $numAudioSelects = param('numaudioselects') || 1;
        }
        my $mccf = create_multicam_config(trim(param("name")), param("numvideoselects"), 
            $numAudioTracks, $numAudioSelects);

        my $mccfId = save_multicam_config($dbh, $mccf) 
            or return_create_page("failed to save multi-camera config to database: $prodautodb::errstr");
        
        redirect_to_page("editmccf.pl?id=$mccfId");
    }
}

return_create_page($errorMessage);



sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Invalid number of video selects" 
        if (!param("numvideoselects") 
            || param("numvideoselects") !~ /^\d+$/
            || param("numvideoselects") <= 0);
        
    return "Error: Invalid number of audio tracks" 
        if (param("numaudiotracks") 
            && param("numaudiotracks") !~ /^\d+$/);

    return "Error: Invalid number of audio selects" 
        if (param("numaudiotracks")
            && (!param("numaudioselects") 
                || param("numaudioselects") !~ /^\d+$/
                || param("numaudioselects") <= 0));

    return undef;
}

sub return_create_page
{
    my ($errorMessage) = @_;
    
    my $page = get_create_content($errorMessage) or
        return_error_page("failed to fill in content for create multi-camera config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_content
{
    my ($message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new multi-camera clip definition"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','createmccf')"}));

    push(@pageContent, p("Name", textfield("name")));

    push(@pageContent, p("# Video selects", textfield("numvideoselects")));
    
    push(@pageContent, p("# Audio tracks", textfield("numaudiotracks")));

    push(@pageContent, p("# Audio selects", textfield("numaudioselects")));
    
    push(@pageContent, submit({-onclick=>"whichPressed=this.name", -name=>"Create"}), span(" "), submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}));

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


