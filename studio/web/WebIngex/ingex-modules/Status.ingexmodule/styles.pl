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
use lib "../../ingex-config";

my $retval = "";

$retval = build_css();

#print "Content-Type: text/css; charset=ISO-8859-1\n\n";
print $retval;
exit(0);

sub build_css
{
	my $f;
	my @allCSS;
	my $css;
	
	if($css = getCSS('/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/ModuleStyles.css')){
		push(@allCSS, $css);
	}
	
	my $dirtoget="/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/monitors/";
	opendir(MONITORS, $dirtoget) or die("Cannot open monitors directory");
	my @thefiles= readdir(MONITORS);
	closedir(MONITORS);
	
	foreach $f (@thefiles) {
		if(substr($f, -4) eq ".css"){
			if($css = getCSS('/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/monitors/'.$f)){
				push(@allCSS, $css);
			}
		}
	}
	
	return join("\n\n",@allCSS);
}

sub getCSS
{
	(my $file) = @_;
	my @css;
	my $line;
	open (FUNCTFILE, '<'.$file) or return 0;
	while ($line = <FUNCTFILE>) {
		push(@css, $line);
	}
	close (FUNCTFILE);
	return join("",@css);
}