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
use ingexhtmlutil;
use IngexJSON;
use LWP::UserAgent;
use HTTP::Request;


print header;

my $dirtoget='./monitors/';
opendir(MONITORS, $dirtoget) or returnError();
my @thefiles = readdir(MONITORS);
closedir(MONITORS);

my %monitors;

my $instanceString = param('instances');
my @instancesToGet = split(/\,/, $instanceString);

if(param('monitor') eq "ingex-all"){
	my $f;
	foreach $f (@thefiles) { 
		if(substr($f, -16) eq '.ingexmonitor.pl'){
			substr($f, -16)  = '';
			$monitors{$f} = getMonitor($f);
		}
	}
} else {
	my $monitorString = param('monitor');
	my @monitorsToGet = split(/\,/, $monitorString);
	my $m;
	my $action = param('action');
	foreach $m (@monitorsToGet){
		$monitors{$m} = getMonitor($m,$action);
	}
}

print hashToJSON(%monitors);
exit(0);


sub returnError
{
	exit(0);
}

sub getMonitor
{
	my $monitor = $_[0];
	my $action = $_[1];
	my %retval;
	my $inst = "";
	my $i;
	my $instance;
	my $objUserAgent = LWP::UserAgent->new;
	$objUserAgent->agent("IngexWeb/1.0 ");
	foreach my $i (@instancesToGet){
		my @iBits = split(/\:/, $i);
		$instance = $iBits[1];
		if(param('queryType') eq 'http' ){
			#issue http request
			my $url = "http://".$instance.param('queryLoc');
			if($action ne "basicInfo") {
				$url .= "/".$action;
			}
			my $objRequest = HTTP::Request->new(POST => $url);
			$objRequest->content_type('application/x-www-form-urlencoded');
			$objRequest->content('query=libwww-perl&mode=dist');
			my $objResponse = $objUserAgent->request($objRequest);
			if (!$objResponse->is_error) {
			  $inst =  $objResponse->content;
			} else {
				$inst = '{"requestError":"Error making http request to <a href=\''.$url.'\' target=\'_blank\'>'.$url.'</a><br />Is the PC on? Is the port accepting requests?<br />Try clicking the link to see if you get some data (may only work if viewing this page from the web server).<br />Once the problem is fixed, refresh this page to re-try.","state":"error"}';
			}
		} else {
			$ENV{"PATH"} = "";
			my $monUntainted;
			if ($monitor =~ /(\w*)/) {
			    $monUntainted = $1;
			}
			my $instUntainted;
			if ($instance =~ /(\w*)/) {
			    $instUntainted = $1;
			}
			my $actionUntainted;
			if ($action =~ /(\w*)/) {
			    $actionUntainted = $1;
			}
			if($instance eq "localhost") {
				$inst = `./monitors/$monUntainted.ingexmonitor.pl $actionUntainted`;
			} else {
				$inst = `ssh ingexWeb\@$instUntainted monitors/$monUntainted.ingexmonitor.pl $actionUntainted`;
			}
		}
		$retval{$iBits[0]} = "json:".$inst;
	}
	return \%retval;
}