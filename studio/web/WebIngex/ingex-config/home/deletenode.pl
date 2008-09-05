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
use lib "..";
use ingexconfig;
use prodautodb;
use ingexdatautil;
use ingexhtmlutil;



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


my $nodeId = param('id');
my $node;

if (defined param('Cancel'))
{
    redirect_to_page("config.pl");
}
elsif (defined param('Delete'))
{
    prodautodb::delete_node($dbh, $nodeId) or
        return_error_page("failed to delete node with id=$nodeId from database: $prodautodb::errstr");    

    redirect_to_page("config.pl");
}
else
{
    $node = prodautodb::load_node($dbh, $nodeId) or
        return_error_page("failed to find node with id=$nodeId from database: $prodautodb::errstr");
}

my $page = get_delete_content($node) or
    return_error_page("failed to fill in content for delete source config page");
   
print header;
print $page;

exit(0);



sub get_delete_content
{
    my ($node) = @_;
    
    my @pageContent;

    my $nodeHTML = ingexhtmlutil::get_node($node);
    
    push(@pageContent, h1('Delete Node'),
        start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','deletenode')"}),
        hidden(-name=>"id", -default=>$node->{'ID'}),
        p("Please confirm"),
        submit({-onclick=>"whichPressed=this.name", -name=>"Delete"}),
        span(' '),
        submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        end_form,
        h2($node->{'NAME'}),
        $nodeHTML);
    
    
    return join('',@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


