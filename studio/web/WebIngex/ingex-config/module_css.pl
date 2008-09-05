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

my $retval = "";

if (param('tab')){
	if(my $css = getCSS(param('tab'))){
		$retval = $css;
	} else {
		$retval = "";
	}
} else {
	$retval = buildCSS();
}

print "Content-Type: text/css; charset=ISO-8859-1\n\n";
print $retval;
exit(0);

sub buildCSS
{
	my $dirtoget="../ingex-modules/";
	opendir(MODULES, $dirtoget) or die("Cannot open modules directory");
	my @thefiles= readdir(MODULES);
	closedir(MODULES);
	
	my $f;
	my @styles;
	my $css;
	
	foreach $f (@thefiles) {
		if(substr($f, -12) eq ".ingexmodule"){
			substr($f, -12)  = "";
			if($css = getCSS($f)){
				push(@styles, $css);
			}
		}
	}
	return join("\n\n",@styles);
}

sub getCSS
{
	(my $module) = @_;
	my @styles;
	my $line;
	my $filefound = 1;
	open (FUNCTFILE, '<../ingex-modules/'.$module.'.ingexmodule/styles.css') or $filefound = 0;
	if($filefound == 1) {
		while ($line = <FUNCTFILE>) {
			push(@styles, $line);
		}
		close (FUNCTFILE);
		return join("",@styles);
	}
	$filefound = 1;
	open (FUNCTFILE, '<../ingex-modules/'.$module.'.ingexmodule/styles.pl') or $filefound = 0;
	if($filefound == 1) {
		$ENV{"PATH"} = "";
		if ($module =~ /(\w*)/) {
	    	$retval = `../ingex-modules/$1.ingexmodule/styles.pl`;
		}
		return $retval;
	}
	return 0;
}