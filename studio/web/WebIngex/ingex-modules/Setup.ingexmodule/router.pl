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
    
    

my $rocfs = load_router_configs($dbh) 
    or return_error_page("failed to load router configs: $prodautodb::errstr");


my $page = get_page_content($rocfs)
    or return_error_page("failed to fill in content for router page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($rocfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Router configurations"));

    push(@pageContent, p(a({-href=>"javascript:getContent('createrocf')"}, "Create new router configuration")));

	# list of configs
    my @rocfList;
    my $nodeConfig;
    my $rocfIndex = 0;
    my $rocf;

    foreach $rocf (@{$rocfs})
    {    
        
        push(@rocfList, 
            li(a({-href=>"javascript:show('RouterConfig-$rocfIndex',true)"}, $rocf->{"config"}->{"NAME"})));
            
       $rocfIndex++;
    }

    push(@pageContent, ul(@rocfList));

	# each config
    $rocfIndex = 0;
    foreach my $rocf (
        sort { $a->{"config"}->{"NAME"} cmp $b->{"config"}->{"NAME"} } 
        (@{ $rocfs })
    )
    {    
        my $rocfHTML = htmlutil::get_router_config($rocf);
        
        push(@pageContent, 
            div({-id=>"RouterConfig-$rocfIndex",-class=>"hidden simpleBlock"},
            h2("$rocf->{'config'}->{'NAME'}"),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editrocf','id=$rocf->{'config'}->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deleterocf','id=$rocf->{'config'}->{'ID'}')"}, "Delete")),
            ), 
            $rocfHTML));

        $rocfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


