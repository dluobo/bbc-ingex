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
use ingexhtmlutil;

my $page = get_page_content() or return_error_page("failed to fill in content for index page");

print header;
print $page;

exit(0);

sub get_page_content
{
    my @pageContent;
	push(@pageContent,h1("Welcome to Ingex."));
	push(@pageContent,p("Welcome to the Ingex tapeless production system. This web interface allows you to manage and monitor your Ingex system, from ensuring that everything is working correctly to setting up recording parameters."));
	push(@pageContent,p("An overview of your Ingex system is below. You should ensure that the nodes displayed match up with the different computers which make up your Ingex network. (You can change the node setup by clicking 'Configure' at the top of this page.) If the nodes you see are correct, you will be able to examine whether they are available for monitoring at this time. If all nodes are available, you will be able to see accurate Status information about your whole system by clicking the 'Status' tab at the top of the page."));
	push(@pageContent,div({id=>"systemInfo"},"<div class='center'><img src='/ingex/img/loading.gif' class='loadingIcon' /></div>"));
	return join("",@pageContent);	
}


