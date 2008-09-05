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
 	push(@pageContent,h1("Help"));
	push(@pageContent,p("Welcome to Ingex, the tapeless production system."));
	push(@pageContent,p("The Ingex interface is divided into different tabs (also called modules), which you can see at the top of this page. Select a tab for the action you require, such as Status to monitor the Ingex system or Material to review the footage logged in Ingex's database. A menu on the left will then provide links to pages for specific tasks, such as exporting material or viewing detail from a specific status monitor."));
	push(@pageContent,p("You can also see basic system state data at the top of the page. A green status icon indicates that Ingex appears to be operating normally, while a yellow or red icon may indicate a problem. Click the icon to learn more. Next to this state icon is information about the number of video inputs connected, and - if applicable - the number of inputs currently being recorded. This data may be useful for checking at-a-glance that everything is as you expect."));
	return join("",@pageContent);	
}


