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
    
    
my $vrs = load_video_resolutions($dbh) 
    or return_error_page("failed to load video resolutions: $prodautodb::errstr");

my $recs = load_recorders($dbh) 
    or return_error_page("failed to load recorders: $prodautodb::errstr");

my $rcfs = load_recorder_configs($dbh) 
    or return_error_page("failed to load recorder configs: $prodautodb::errstr");


my $page = get_page_content($recs, $rcfs, $vrs) 
    or return_error_page("failed to fill in content for recorder page");
   
print header;
print $page;

exit(0);

    
sub get_page_content
{
    my ($recs, $rcfs, $vrs) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Recorders"));

    
    push(@pageContent, p(a({-href=>"javascript:getContent('createrec')"}, "Create new")));
    
    
    my @recorderRows;
    foreach my $rec (@{ $recs })
    {    
        my @configRows;

        foreach my $rcf (@{ $rcfs })
        {    
            my $rcfConfig = $rcf->{"config"};
            
            if ($rec->{"ID"} == $rcfConfig->{"RECORDER_ID"})
            {
                if (defined $rec->{"CONF_ID"} && $rec->{"CONF_ID"} == $rcfConfig->{"ID"})
                {
                    unshift(@configRows,
                        Tr({-align=>"left", -valign=>"top"}, [
                            td([div({-class=>"propHeading2"}, "config:"), 
                                a({-href=>"javascript:show('RecorderConfig-$rcfConfig->{'ID'}',true)"}, $rcfConfig->{"NAME"}),
                            ]),
                        ])
                    );
                }
                else
                {
                    push(@configRows,
                        Tr({-align=>"left", -valign=>"top"}, [
                            td([div({-class=>"propHeading2"}, "alternative:"), 
                                a({-href=>"javascript:show('RecorderConfig-$rcfConfig->{'ID'}',true)"}, $rcfConfig->{"NAME"}),
                            ]),
                        ])
                    );
                }
            }
        }
        
        if (!defined $rec->{"CONF_ID"})
        {
            # not connected
            unshift(@configRows,
                Tr({-align=>"left", -valign=>"top"}, [
                    td([div({-class=>"propHeading2"}, "config:"), i("not set")]), 
                ])
            );
        }
        
        push(@recorderRows, 
            Tr({-align=>"left", -valign=>"top"},
                td(b($rec->{"NAME"})),
                td(
                    small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editrec','id=$rec->{'ID'}')"}, "Edit")),
                    small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deleterec','id=$rec->{'ID'}')"}, "Delete")),
                ),
            ),
			Tr({-align=>"left", -valign=>"top"},
                td(""),
                td(small(a({-href=>"javascript:getContentWithVars('creatercf','recid=$rec->{'ID'}')"}, "Create new config"))),
            ),
            Tr({-align=>"left", -valign=>"top"},
                td(""),
                td(table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @configRows)),
            ),
        );
    }
    
    push(@pageContent, table({-border=>0, -cellspacing=>3,-cellpadding=>3}, @recorderRows));


    # each recorder config    
    foreach my $rcf (
        sort { $a->{"config"}->{"RECORDER_NAME"} cmp $b->{"config"}->{"RECORDER_NAME"} } 
        (@{ $rcfs })
    )
    {    
        my $rcfHTML = htmlutil::get_recorder_config($rcf, $vrs);
        
        my $rcfConfig = $rcf->{"config"};
        
        push(@pageContent, 
            div({-id=>"RecorderConfig-$rcfConfig->{'ID'}",-class=>"hidden simpleBlock"},
            h2("$rcf->{'config'}->{'RECORDER_NAME'} - $rcf->{'config'}->{'NAME'}"),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editrcf','id=$rcf->{'config'}->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deletercf','id=$rcf->{'config'}->{'ID'}')"}, "Delete")),
            ), 
            $rcfHTML));
    }
        
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


