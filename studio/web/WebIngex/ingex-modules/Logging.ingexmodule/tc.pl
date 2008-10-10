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
use LWP::UserAgent;
use HTTP::Request;
use strict;

print header;
print getData();
exit(0);

sub getData
{
	my $retval;
	my $objUserAgent = LWP::UserAgent->new;
	$objUserAgent->agent("IngexWeb/1.0 ");
	
	#issue http request
	my $url = "http://".param('url');
	my $objRequest = HTTP::Request->new(GET => $url);
	$objRequest->content_type('application/x-www-form-urlencoded');
	$objRequest->content('query=libwww-perl&mode=dist');
	
	my $objResponse = $objUserAgent->request($objRequest);
	if (!$objResponse->is_error) {
	  $retval =  $objResponse->content;
	} else {
		$retval = '{"requestError":"Error making http request to <a href=\''.$url.'\' target=\'_blank\'>'.$url.'</a><br />Is the PC on? Is the port accepting requests?<br />Try clicking the link to see if you get some data (may only work if viewing this page from the web server).<br />Once the problem is fixed, refresh this page to re-try.","state":"error"}';
	}
	return $retval;
}