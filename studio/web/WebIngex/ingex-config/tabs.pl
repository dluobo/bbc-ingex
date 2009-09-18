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

use CGI::Pretty qw(:standard);
use strict;

use lib ".";
use lib "..";

my $retval;

if (param('tab')){
	$retval = one_tab(param('tab'));
} else {
	$retval  = build_tabs();
}

print header;
print $retval;
exit(0);

sub build_tabs
{
	my $dirtoget="../ingex-modules/";
	opendir(MODULES, $dirtoget) or die("Cannot open modules directory");
	my @thefiles= readdir(MODULES);
	closedir(MODULES);
	
	my @tabsHTML;
	push(@tabsHTML, "<ul>");
	
	my $f;
	
	foreach $f (@thefiles) {
		if(substr($f, -12) eq ".ingexmodule" && $f ne "OldMaterial.ingexmodule"){
			substr($f, -12)  = "";
			push(@tabsHTML, "<li class='tab' id='$f\_tab'><a href='javascript:getTab(\"$f\",false,true,true)'>$f</a></li>");
		}
	}
	
	push(@tabsHTML, "<ul>");
	return join("\n",@tabsHTML);
}

sub one_tab
{
	(my $f) = @_;
	return "<ul><li class='tab' id='$f\_tab'><a href='javascript:getTab(\"$f\",false,true,true)'>$f</a></li></ul>";
}