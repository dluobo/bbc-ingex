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
    redirect_to_page("prog.pl");
}
	
if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

my $Id = param('id');
my $prog;
my $errorMessage;

if (defined param('Reset'))
{
    Delete_all();
    $prog = db::load_programme($dbh, $Id) or
        return_error_page("failed to find programme with id=$Id from database: $prodautodb::errstr");
}
elsif (defined param('Done'))
{
    $prog = db::load_programme($dbh, $Id) or
        return_error_page("failed to load programme with id=$Id from database: $prodautodb::errstr");

    if (!defined param('name') || param('name') =~ /^\s*$/)
    {
        $errorMessage = "Error: Empty name"; 
    }
    elsif (!defined param('series'))
	{
		$errorMessage = "Error: Series not defined";
	}
	else
    {
        $prog->{'NAME'} = trim(param('name'));
		$prog->{'SERIESID'} = param('series');
        
        db::update_programme($dbh, $prog) or 
            return_error_page("failed to update programme to database: $prodautodb::errstr");
        
        redirect_to_page("prog.pl");
    }
}
else
{
    $prog = db::load_programme($dbh, $Id) or
        return_error_page("failed to load programme with id=$Id from database: $prodautodb::errstr");
}


my $series = db::load_series($dbh) 
    or return_error_page("failed to load series list from database: $prodautodb::errstr");

my $page = get_content($prog, $series, $errorMessage) or
    return_error_page("failed to fill in content for programme page");
   
print header;
print $page;

exit(0);



sub get_content
{
    my ($prog, $series, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1('Edit programme'));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editprog')"}));

    push(@pageContent, hidden('id', $prog->{'ID'}));

    push(@pageContent, p(textfield('name', $prog->{'NAME'})));

	my @values;
    my %labels;
	my $default;
    foreach my $ser ( @{ $series } )
    {
        push(@values, $ser->{"ID"});
        $labels{$ser->{"ID"}} = $ser->{"NAME"};
        $default = $prog->{"SERIESID"};
    }
    
   push(@pageContent, p('Series', popup_menu(
        -name=>'series',
        -default=>$default,
        -values=>\@values,
        -labels=>\%labels
    )));
    
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


