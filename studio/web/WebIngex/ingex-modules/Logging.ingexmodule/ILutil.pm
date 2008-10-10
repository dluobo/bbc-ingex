# Copyright (C) 2008  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
# Modified by: Rowan de Pomerai <rdepom@users.sourceforge.net>
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
 
package ILutil;

use strict;
use warnings;

use CGI::Pretty qw(:standard);

####################################
#
# Module exports
#
####################################

BEGIN 
{
    use Exporter ();
    our (@ISA, @EXPORT);

    @ISA = qw(Exporter);
    @EXPORT = qw(
        &toTC
    );
}

sub toTC {
	my ($frames, $editrate) = @_;
	if(!defined $editrate) { $editrate = 25; }
	my $timecode;
	my $h;
	my $m;
	my $s;
	my $f;
	
	$s = int($frames/$editrate);
	$m = int($s/60);
	$h = int($m/60);
	
	$f = $frames - ($editrate * $s);
	$s = $s - (60 * $m);
	$m = $m - (60 * $h);
	
	if ($h < 10) { $h = "0".$h; }
	if ($m < 10) { $m = "0".$m; }
	if ($s < 10) { $s = "0".$s; }
	if ($f < 10) { $f = "0".$f; }
	
	$timecode = "$h:$m:$s:$f";
	
	return $timecode;
}

1;
