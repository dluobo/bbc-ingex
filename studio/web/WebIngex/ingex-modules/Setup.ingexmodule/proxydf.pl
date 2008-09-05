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
    
    

my $pxdfs = load_proxy_defs($dbh) 
    or return_error_page("failed to load proxy definitions: $prodautodb::errstr");


my $page = get_page_content($pxdfs)
    or return_error_page("failed to fill in content for proxy definitions page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($pxdfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Proxy definitions"));

    
    push(@pageContent, p(a({-href=>"javascript:getContent('createproxydf')"}, "Create new")));
    
    
    # list of proxy defs

    my @pxdfList;
    my $pxdfIndex = 0;
    
    foreach my $pxdf ( @{ $pxdfs } )
    {    
        push(@pxdfList, 
            li(a({-href=>"javascript:show('ProxyDef-$pxdfIndex',true)"}, $pxdf->{"def"}->{"NAME"})));
            
       $pxdfIndex++;
    }

    push(@pageContent, ul(@pxdfList)); 

    
    # each proxy def
    $pxdfIndex = 0;
    foreach my $pxdf ( @ { $pxdfs } )
    {    
        my $pxdfHTML = htmlutil::get_proxy_def($pxdf);
        
        push(@pageContent, div({-id=>"ProxyDef-$pxdfIndex",-class=>"simpleBlock"},
            h2($pxdf->{"def"}->{"NAME"}),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editproxydf','id=$pxdf->{'def'}->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deleteproxydf','id=$pxdf->{'def'}->{'ID'}')"}, "Delete")),
            ), 
            $pxdfHTML
        ));

        $pxdfIndex++;
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


