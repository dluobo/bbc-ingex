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
 
package ingexhtmlutil;

use strict;
use warnings;
use ingexconfig;

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
        &trim
        &get_html_param_id
        &redirect_to_page
        &return_error_page
		&get_node
		&log_message
    );
}

####################################
#
# Remove leading and trailing spaces
#
####################################

sub trim
{
    my ($value) = @_;
    
    $value =~ s/^\s*(\S*(?:\s+\S+)*)\s*$/$1/;
    
    return $value;
}

####################################
#
# Create an id for HTML form element
#
####################################

sub get_html_param_id
{
    my ($names, $ids) = @_;
    
    return join("-", join("-", @{ $names }), join("-", @{ $ids }));
}

####################################
#
# Redirect page
#
####################################

sub redirect_to_page
{
    my ($pageName) = @_;
    
    my $relUrl = url(-relative=>1);
    my $newUrl = url();
    $newUrl =~ s/^(.*)$relUrl$/$1$pageName/;
    
    print redirect($newUrl);

    exit(0);
}

####################################
#
# Error page
#
####################################

sub return_error_page
{
    my ($message) = @_;

    my @pageContent = h1("Error");
    push(@pageContent, p({-style=>"color: red;"}, $message));
    
    my $page = join("", @pageContent);

    print header;
    print $page;

    exit(0);    
}

####################################
#
# Node config
#
####################################

sub get_node
{
    my ($nodeConfig) = @_;
    
    my @pageContent;

    my @tableRows;
    
    push(@tableRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Name:"), $nodeConfig->{"NAME"}]),
        ]));
        
    push(@tableRows,
        Tr({-align=>"left", -valign=>"top"}, [
           td([div({-class=>"propHeading1"}, "Type:"), $nodeConfig->{"TYPE"}]),
        ]));
   
	push(@tableRows,
    	Tr({-align=>"left", -valign=>"top"}, [
	    	td([div({-class=>"propHeading1"}, "Host/IP:"), $nodeConfig->{"IP"}]),
	    ]));
	
	push(@tableRows,
    	Tr({-align=>"left", -valign=>"top"}, [
	    	td([div({-class=>"propHeading1"}, "Monitored Volumes:"), $nodeConfig->{"VOLUMES"}]),
	    ]));
    
    push(@pageContent, table({-class=>"noBorderTable"},
        @tableRows));

    return join("", @pageContent);
}

####################################
#
# Error log
#
####################################

# write a string to the log file
sub log_message 
{
	my ($logmsg) = @_;
	my $logfile = ">>".$ingexConfig{'ingex_log'};
	sub LOCK_EX()  { 2 }     #  Exclusive lock (for writing)
	
	if($logmsg){
		($logmsg) = ($logmsg =~ /((.|\n)*)/); 	#untaint
		
		if(open(LOG, $logfile))
		{
			flock(LOG, LOCK_EX);	# gain exclusive lock
			my ($sec, $min, $hour, $day, $mon, $year) = localtime(time);
			printf LOG "[%04d/%02d/%02d-%02d:%02d:%02d] - $logmsg\n", $year+1900, $mon+1, $day, $hour, $min, $sec;
			close LOG;
		}
	}
}

1;
