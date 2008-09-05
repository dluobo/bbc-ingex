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
use htmlutil; use ingexhtmlutil;;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();


my $scfs = load_source_configs($dbh) 
    or return_error_page("failed to load source configs: $prodautodb::errstr");

    
my $page = get_page_content(\$scfs)
    or return_error_page("failed to fill in content for source config page");
   
print header;
print $page;

exit(0);



sub get_page_content
{
    my ($scfs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Source group definitions"));

    
    push(@pageContent, p(a({-href=>"javascript:getContent('createscf')"}, "Create new")));
    
    
    # list of source configs

    my @scfList;
    my $scfConfig;
    my $scfIndex = 0;
    my $scf;
    
    foreach $scf (@{$$scfs})
    {    
        $scfConfig = $scf->{"config"};
        
        push(@scfList, 
            li(a({-href=>"javascript:show('SourceConfig-$scfIndex',true)"}, $scfConfig->{"NAME"})));
            
       $scfIndex++;
    }

    push(@pageContent, ul(@scfList)); 

    
    # each source config
    
    $scfIndex = 0;
    foreach $scf (@{$$scfs})
    {    
        my $scfHTML = htmlutil::get_source_config($scf);
        
        $scfConfig = $scf->{"config"};
        
        push(@pageContent, div({-id=>"SourceConfig-$scfIndex",-class=>"hidden simpleBlock"},
            h2($scf->{"config"}->{"NAME"}),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editscf','id=$scf->{'config'}->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deletescf','id=$scf->{'config'}->{'ID'}')"}, "Delete")),
            ), 
            $scfHTML));

        $scfIndex++;
    }
        
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


