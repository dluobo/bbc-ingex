#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
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

if (defined param('Cancel'))
{
    redirect_to_page("proj.pl");
}
	
if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

my $Id = param('id');
my $proj;
my $errorMessage;

if (defined param('Reset'))
{
    Delete_all();
    $proj = db::load_project($dbh, $Id) or
        return_error_page("failed to find project name with id=$Id from database: $prodautodb::errstr");
}
elsif (defined param('Done'))
{
    $proj = db::load_project($dbh, $Id) or
        return_error_page("failed to load project name with id=$Id from database: $prodautodb::errstr");

    if (!defined param('name') || param('name') =~ /^\s*$/)
    {
        $errorMessage = "Error: Empty name"; 
    }
    else
    {
        $proj->{'NAME'} = trim(param('name'));
        
        update_project($dbh, $proj) or 
            return_error_page("failed to update project name to database: $prodautodb::errstr");
        
        redirect_to_page("proj.pl");
    }
}
else
{
    $proj = db::load_project($dbh, $Id) or
        return_error_page("failed to load project name with id=$Id from database: $prodautodb::errstr");
}


my $page = get_content($proj, $errorMessage) or
    return_error_page("failed to fill in content for project name page");
   
print header;
print $page;

exit(0);



sub get_content
{
    my ($proj, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit project name'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editproj')"}));

    push(@pageContent, hidden('id', $proj->{'ID'}));

    push(@pageContent, p(textfield('name', $proj->{'NAME'})));
    
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


