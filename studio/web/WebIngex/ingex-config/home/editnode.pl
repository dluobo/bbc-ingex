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


if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

if (defined param('Cancel'))
{
    redirect_to_page("config.pl");
}

my $nodeId = param('id');
my $errorMessage;

my @nodeTypes = split(/\s*\,\s*/, $ingexConfig{"node_types"}) 
    or return_error_page("failed to load node types");

my $node = load_node($dbh, $nodeId) or
    return_error_page("failed to find node with id=$nodeId from database: $prodautodb::errstr");

    
if (defined param('Reset'))
{
    Delete_all();
}
elsif (defined param('Done'))
{
    if (!($errorMessage = validate_params($nodeId, \@nodeTypes)))
    {   
     	$node->{"NAME"} = param("name");
        $node->{"TYPE"} = param("type");
		$node->{"IP"} = param("ip");
		$node->{"VOLUMES"} = param("volumes");
        if (!update_node($dbh, $node))
        {
            # do same as a reset
            Delete_all();
            $node = load_node($dbh, $nodeId) or
                return_error_page("failed to find node with id=$nodeId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update node in database: $prodautodb::errstr";
        }
        else
        {
            $node = load_node($dbh, $nodeId) or
                return_error_page("failed to reload saved node from database: $prodautodb::errstr");
    
            redirect_to_page("config.pl");
        }
        
    }
}


return_edit_page($node, \@nodeTypes, $errorMessage);




sub validate_params
{
    my ($node, $nodeTypes) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    return "Error: missing type" if (!defined param("type"));
    return "Error: missing hostname/ip" if (!defined param("ip") || param("ip") =~ /^\s*$/);
    
    return undef;
}

sub return_edit_page
{
    my ($node, $nodeTypes, $errorMessage) = @_;
    
    my $page = get_content($node, $nodeTypes, $errorMessage) or
        return_error_page("failed to fill in content for edit node page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_content
{
    my ($node, $nodeTypes, $errorMessage) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Edit node definition"));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editnode')"}));

    push(@pageContent, hidden("id", $node->{"ID"}));

    my @topRows;

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Name:'), 
            textfield('name', $node->{'NAME'})]),
        ]));


 	   my @values;
	    my %labels;
	    foreach my $nodeType (@{$nodeTypes})
	    {
	        push(@values, $nodeType);
	        $labels{$nodeType} = $nodeType;
	    }
	   	 push(@topRows,  
		        Tr({-align=>'left', -valign=>'top'}, 
						[
		            		td(
								[div({-class=>"propHeading1"}, 'Type:'), 
		                popup_menu(
			                -name=>"type",
			                -values=>\@values,
			                -labels=>\%labels,
							 -default=>$node->{'TYPE'}
			            ),
		        ])]));

			push(@topRows,  
			   Tr({-align=>'left', -valign=>'top'}, [
			       td([div({-class=>"propHeading1"}, 'Hostname or IP address:'), 
	            		textfield('ip', $node->{'IP'})]),
		        ]));
			
			push(@topRows,  
		        Tr({-align=>'left', -valign=>'top'}, [
		            td([div({-class=>"propHeading1"}, 'Monitored Volumes:'), 
		            textfield('volumes', $node->{'VOLUMES'})]),
		        ]));


		    push(@pageContent, table({-class=>"noBorderTable"},
		        @topRows));


		    push(@pageContent, 
		        p(
		            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
		            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}),
		            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
		        )
		    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


