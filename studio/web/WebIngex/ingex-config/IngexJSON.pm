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

package IngexJSON;

use strict;
use Scalar::Util 'reftype';

BEGIN 
{
    use Exporter ();
    our (@ISA, @EXPORT);

    @ISA = qw(Exporter);
    @EXPORT = qw(
        &hashToJSON
		&arrayToJSON
		&elementToJSON
    );
}

sub hashToJSON
{
	my %data = @_;
	my $json = '{';
	my $d;
	
	foreach $d (keys %data) {
		$json .= '"';
		$json .= $d;
		$json .= '":';		
		$json .= elementToJSON($data{$d});
		$json .= ',';
	}
	
	substr($json, -1)  = '' if substr($json, -1) eq ',';
	$json .= '}';
	return $json;
}

sub arrayToJSON
{
	my @data = @_;
	my $json = '[';
	my $d;
	
	foreach $d (@data) {
		$json .= elementToJSON($d).",";
	}
	
	substr($json, -1)  = '' if substr($json, -1) eq ',';
	$json .= ']';
	
	return $json;
}

sub elementToJSON
{
	my $d = $_[0];
	my $json;
	
	#find the reference type for $d
	my $reftype = reftype $d;
	if ( defined $reftype) {
		if ( $reftype eq 'HASH' ) {
		    #if it's a hash, call hashToJSON with the dereferenced hash
			$json .= hashToJSON(%$d);
		}
		elsif ( $reftype eq 'ARRAY' ) {
		    #if it's an array, call arrayToJSON with the dereferenced array
			$json .= arrayToJSON(@$d);
		}
		elsif ( $reftype eq 'SCALAR' ) {
			#if it's a reference to a scalar, dereference it
			$json .= quotedScalar($$d);
		}
	} else {
		#if it's not a reference, assume it's a scalar
		$json .= quotedScalar($d);
	}

	return $json;
}

sub quotedScalar
{
	my $d = $_[0];
	my $retval;
	if (substr($d,0,5) eq 'json:'){
		substr($d,0,5) = '';
		$retval = $d;
	} else {
		$retval = '"';
		$retval .= $d;
		$retval .= '"';
	}
	return $retval;
}

1;