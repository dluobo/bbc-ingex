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


my $projects = db::load_projects($dbh) or 
    return_error_page("failed to load recorder locations: $prodautodb::errstr");


my $page = get_page_content($projects) or
    return_error_page("failed to fill in content for general configuration page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($projects) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1('Projects'));

    push(@pageContent, p(a({-href=>"javascript:getContentWithVars('createproj','type=recloc')"}, "Create new")));
    
 
    my @tableRows;
    push(@tableRows, 
        Tr({-align=>'left', -valign=>'top'}, [
           th(['Name',
            '',
            '']),
        ]));
    foreach my $proj (@{$projects})
    {    
        # if ($proj->{'ID'} == 1) # id=1 is a reserved value ("Unspecified")
        # {
        #     push(@tableRows, 
        #         Tr({-align=>'left', -valign=>'top'}, [
        #            td([$proj->{'NAME'},
        #             '',
        #             '']),
        #         ]));
        # }
        # else
        # {
            push(@tableRows, 
                Tr({-align=>'left', -valign=>'top'}, [
                   td([$proj->{'NAME'},
                    small(a(({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editproj','id=$proj->{'ID'}')"}, 'Edit'))),
                    small(a(({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deleteproj','id=$proj->{'ID'}')"}, 'Delete')))]),
                ]));
        # }
    }

    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3},
        @tableRows));

    
    
    return join('',@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


