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
use ingexhtmlutil;


my $page = get_page_content()
	or return_error_page("Failed to fill in content for page");

print header;
print $page;

exit(0);

sub get_page_content
{
	my @pageContent;

	#push(@pageContent, h1("Ingex Status"));
	push(@pageContent, div({-id=>"monitorsContainer"},p("Loading monitors...")));
			
	#return the page HTML, first joining it to a scalar variable
	return join("",@pageContent);
}