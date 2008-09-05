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
    
    

my $mccfs = load_multicam_configs($dbh) 
    or return_error_page("failed to load multicam configs: $prodautodb::errstr");


my $page = get_page_content(\$mccfs) 
    or return_error_page("failed to fill in content for multicam config page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($mccfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Multi-camera clip definitions"));

    
    push(@pageContent, p(a({-href=>"javascript:getContent('createmccf')"}, "Create new")));
    
    
    # list of multi-cam configs

    my @mccfList;
    my $mccfConfig;
    my $mccfIndex = 0;
    my $mccf;
    
    foreach $mccf (@{$$mccfs})
    {    
        $mccfConfig = $mccf->{"config"};
        
        push(@mccfList, 
            li(a({-href=>"javascript:show('RecorderConfig-$mccfIndex',true)"}, $mccfConfig->{"NAME"})));
            
       $mccfIndex++;
    }

    push(@pageContent, ul(@mccfList)); 

    
    # each multi-cam config
    
    $mccfIndex = 0;
    foreach $mccf (@{$$mccfs})
    {    
        my $mccfHTML = htmlutil::get_multicam_config($mccf);
        
        $mccfConfig = $mccf->{"config"};
        
        push(@pageContent, div({-id=>"RecorderConfig-$mccfIndex",-class=>"hidden simpleBlock"},
            h2($mccf->{"config"}->{"NAME"}),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editmccf','id=$mccf->{'config'}->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deletemccf','id=$mccf->{'config'}->{'ID'}')"}, "Delete")),
                ), 
            $mccfHTML));

        $mccfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


