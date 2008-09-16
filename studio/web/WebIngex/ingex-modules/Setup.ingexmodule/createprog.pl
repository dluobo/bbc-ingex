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


my $errorMessage;

if (defined param('Cancel'))
{
    redirect_to_page("prog.pl");
}
elsif (defined param('Create'))
{
        if (!($errorMessage = validate_params()))
        {
            my $name = trim(param('name'));
			my $series = trim(param('series'));
            
            my $Id = db::save_programme($dbh, $name, $series) or 
                return_error_page("failed to save programme '$name' to database: $prodautodb::errstr");
            
            my $rlc = db::load_programme($dbh, $Id) or
                return_error_page("failed to reload saved programme from database: $prodautodb::errstr");
    
            redirect_to_page("prog.pl");
        }
}

return_page_content($errorMessage);




sub validate_params
{
    return "Error: Empty name" if (!defined param('name') || param('name') =~ /^\s*$/);
	return "Error: Missing series identifier" if (!defined param("series"));

    return undef;
}

sub return_page_content
{
    my ($errorMessage) = @_;
    
	my $series = db::load_series($dbh) 
	    or return_error_page("failed to load series list from database: $prodautodb::errstr");

    my $page = get_content($series,$errorMessage) or
        return_error_page("failed to fill in content for create programme page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_content
{
    my ($series,$message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Create new programme'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','createprog')"}));

    push(@pageContent, p('Name', textfield('name')));

    my @values;
    my %labels;
	my $default;
    foreach my $ser ( @{ $series } )
    {
        push(@values, $ser->{"ID"});
        $labels{$ser->{"ID"}} = $ser->{"NAME"};
        $default = $ser->{"ID"};
    }
    
   push(@pageContent, p('Series', popup_menu(
        -name=>'series',
        -default=>$default,
        -values=>\@values,
        -labels=>\%labels
    )));

    push(@pageContent, submit({-onclick=>"whichPressed=this.name", -name=>"Create"}), span(' '), submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}));

    push(@pageContent, end_form);

    
    return join('', @pageContent);
}



END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


