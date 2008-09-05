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


my $mccfId = param('id');
my $mccf;


if (defined param('Cancel'))
{
    redirect_to_page("multicamcf.pl");
}
elsif (defined param('Delete'))
{
    delete_multicam_config($dbh, $mccfId) or
        return_error_page("failed to delete multi-camera config with id=$mccfId from database: $prodautodb::errstr");    

    redirect_to_page("multicamcf.pl");
}
else
{
    $mccf = load_multicam_config($dbh, $mccfId) or
        return_error_page("failed to find multi-camera config with id=$mccfId from database: $prodautodb::errstr");
}


my $page = get_delete_content($mccf) or
    return_error_page("failed to fill in content for delete multi-camera config page");
   
print header;
print $page;

exit(0);





sub get_delete_content
{
    my ($mccf) = @_;
    
    my @pageContent;

    my $mccfHTML = htmlutil::get_multicam_config($mccf);
    
    push(@pageContent, h1("Delete multi-camera configuration"),
        start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','deletemccf')"}),
        hidden(-name=>"id", -default=>$mccf->{"config"}->{"ID"}),
        p("Please confirm"),
        submit({-onclick=>"whichPressed=this.name", -name=>"Delete"}),
        span(" "),
        submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        end_form,
        h2($mccf->{"config"}->{"NAME"}),
        $mccfHTML);
    
    
    return join("",@pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


