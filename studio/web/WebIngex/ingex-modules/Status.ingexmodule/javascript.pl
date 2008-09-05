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

$retval = build_javascript();

#print "Content-Type: text/javascript; charset=ISO-8859-1\n\n";
print $retval;
exit(0);

sub build_javascript
{
	my $f;
	my @javaScript;
	my $js;
	
	if($js = getJS('/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/ModuleJavascript.js')){
		push(@javaScript, $js);
	}
	
	my $dirtoget="/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/monitors/";
	opendir(MONITORS, $dirtoget) or die("Cannot open monitors directory");
	my @thefiles= readdir(MONITORS);
	closedir(MONITORS);
	
	foreach $f (@thefiles) {
		if(substr($f, -3) eq ".js"){
			if($js = getJS('/srv/www/cgi-bin/ingex-modules/Status.ingexmodule/monitors/'.$f)){
				push(@javaScript, $js);
			}
		}
	}
	
	return join("\n\n",@javaScript);
}

sub getJS
{
	(my $file) = @_;
	my @javaScript;
	my $line;
	open (FUNCTFILE, '<'.$file) or return 0;
	while ($line = <FUNCTFILE>) {
		push(@javaScript, $line);
	}
	close (FUNCTFILE);
	return join("",@javaScript);
}