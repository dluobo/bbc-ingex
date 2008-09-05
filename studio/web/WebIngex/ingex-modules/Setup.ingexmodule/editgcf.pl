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
elsif (!defined param('type'))
{
    return_error_page("Missing 'type' parameter");
}
elsif (param('type') ne "recloc")
{
    return_error_page("Only type 'recloc' is supported");
}

my $rlcId = param('id');
my $rlc;
my $errorMessage;

if (defined param('Cancel'))
{
    redirect_to_page("generalcf.pl");
}
elsif (defined param('Reset'))
{
    Delete_all();
    $rlc = db::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to find recording location with id=$rlcId from database: $prodautodb::errstr");
}
elsif (defined param('Done'))
{
    $rlc = db::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to load recording location with id=$rlcId from database: $prodautodb::errstr");

    if (!defined param('name') || param('name') =~ /^\s*$/)
    {
        $errorMessage = "Error: Empty name"; 
    }
    else
    {
        $rlc->{'NAME'} = trim(param('name'));
        
        update_recording_location($dbh, $rlc) or 
            return_error_page("failed to update recording location to database: $prodautodb::errstr");
        
        redirect_to_page("generalcf.pl");
    }
}
else
{
    $rlc = db::load_recording_location($dbh, $rlcId) or
        return_error_page("failed to load recording location with id=$rlcId from database: $prodautodb::errstr");
}


my $page = get_edit_recloc_content($rlc, $errorMessage) or
    return_error_page("failed to fill in content for source config page");
   
print header;
print $page;

exit(0);



sub get_edit_recloc_content
{
    my ($rlc, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit recording location'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editgcf')"}));

    push(@pageContent, hidden('id', $rlc->{'ID'}));
    push(@pageContent, hidden('type', 'recloc'));

    push(@pageContent, p(textfield('name', $rlc->{'NAME'})));
    
    push(@pageContent,
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"})
        )
    );

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


