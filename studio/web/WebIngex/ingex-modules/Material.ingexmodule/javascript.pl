#!/usr/bin/perl -wT

# Copyright (C) 2009  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
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

use lib "../ingex-config";
use lib "../ingex-modules/Material.ingexmodule";
use lib "..";

use prodautodb;
use ingexhtmlutil;
use ingexconfig;
use db;

use CGI::Pretty qw(:standard);
use JSON::XS;

my $retval = "";

$retval = build_javascript();
print $retval;

exit(0);


sub build_javascript
{
	my $f;
	my @javaScript;
	my $js;

	if($js = getJS('/srv/www/cgi-bin/ingex-modules/Material.ingexmodule/UserSettings.js')){
		push(@javaScript, $js);
	}
	if($js = getJS('/srv/www/cgi-bin/ingex-modules/Material.ingexmodule/Ajax.js')){
		push(@javaScript, $js);
	}
	if($js = getJS('/srv/www/cgi-bin/ingex-modules/Material.ingexmodule/Net.js')){
		push(@javaScript, $js);
	}
	if($js = getJS('/srv/www/cgi-bin/ingex-modules/Material.ingexmodule/ModJavascript.js')){
		push(@javaScript, $js);
	}

	return join("\n\n",@javaScript);
}


sub getJS
{
	(my $file) = @_;
	my @javaScript;
	my $line;
	open (FUNCTFILE, '<:encoding(UTF-8)', $file) or return 0;
	while ($line = <FUNCTFILE>) {
		push(@javaScript, $line);
	}
	close (FUNCTFILE);
	return join("",@javaScript);
}






