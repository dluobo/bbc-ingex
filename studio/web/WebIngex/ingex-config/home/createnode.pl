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
use lib "..";
use ingexconfig;
use prodautodb;
use ingexdatautil;
use ingexhtmlutil;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();



if (defined param("Cancel"))
{
    redirect_to_page("config.pl");
}


my @nodeTypes = split(/\s*\,\s*/, $ingexConfig{"node_types"}) 
    or return_error_page("failed to load node types");

my $errorMessage;


if (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $name = trim(param("name"));
		my $ip = trim(param("ip"));
		my $volumes = trim(param("volumes"));
        
        my $node = create_node(param("nodetype"), $name, $ip, $volumes);
        
        my $nodeId = save_node($dbh, $node) 
            or return_create_page(\@nodeTypes, "failed to save node to database: $prodautodb::errstr");
        
        $node = load_node($dbh, $nodeId)
            or return_error_page(\@nodeTypes, "failed to reload saved node from database: $prodautodb::errstr");

        redirect_to_page("config.pl");
    }
}


return_create_page(\@nodeTypes, $errorMessage);




sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
	return "Error: Empty hostname/IP" if (!defined param("ip") || param("ip") =~ /^\s*$/);
	return "Error: Empty type" if (!defined param("nodetype"));

    return undef;
}

sub return_create_page
{
    my ($nodeTypes, $errorMessage) = @_;
    
    my $page = get_create_content($nodeTypes, $errorMessage) 
        or return_error_page("failed to fill in content for create node page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_content
{
    my ($nodeTypes, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new node definition"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','createnode')"}));

    push(@pageContent, p("Name", textfield("name")));

    my @values;
    my %labels;
    foreach my $nodeType (@{$nodeTypes})
    {
        push(@values, $nodeType);
        $labels{$nodeType} = $nodeType;
    }
    push(@pageContent, 
        p(
            "Type", 
            popup_menu(
                -name=>"nodetype",
                -values=>\@values,
                -labels=>\%labels
            )
        )
    );

    push(@pageContent, p("Host name or IP address", textfield("ip")));

	push(@pageContent, p("Monitored Volumes", textfield("volumes")));

    push(@pageContent, 
        submit({-onclick=>"whichPressed=this.name", -name=>"Create"}), 
        submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"})
    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


