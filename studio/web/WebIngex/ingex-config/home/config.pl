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
use ingexhtmlutil;

my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();

my $nodes = load_nodes($dbh) 
    or return_error_page("failed to load nodes: $prodautodb::errstr");

my $page = get_page_content(\$nodes)
    or return_error_page("failed to fill in content for page");

print header;
print $page;

exit(0);



sub get_page_content
{
	my ($nodes) = @_;
    my @pageContent;
    
    push(@pageContent, h1("Node Configuration"));
    
    push(@pageContent, p(a({-href=>"javascript:getContent('createnode')"}, "Create new")));
    
    # list of nodes
    my @nodeList;
    my $nodeConfig;
    my $nodeIndex = 0;
    my $node;

    foreach $node (@{$$nodes})
    {    
        
        push(@nodeList, 
            li(a({-href=>"javascript:show('NodeConfig-$nodeIndex',true)"}, $node->{"NAME"})));
            
       $nodeIndex++;
    }

    push(@pageContent, ul(@nodeList)); 

    
    # each node    
    $nodeIndex = 0;
    foreach $node (@{$$nodes})
    {    
        my $nodeHTML = ingexhtmlutil::get_node($node);
        
        $nodeConfig = $node->{"config"};
        
        push(@pageContent, 
			div({-id=>"NodeConfig-$nodeIndex",-class=>"hidden simpleBlock"},
            h2($node->{"NAME"}),
            p(
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('editnode','id=$node->{'ID'}')"}, "Edit")),
                small(a({-class=>"simpleButton",-href=>"javascript:getContentWithVars('deletenode','id=$node->{'ID'}')"}, "Delete")),
            ), 
            $nodeHTML));

        $nodeIndex++;
    }

	my @confFiles = listFiles();
	my %confFiles;
	foreach my $confFile (@confFiles)
    {
        $confFiles{$confFile} = $confFile;
    }

	push(@pageContent, h1("Advanced : Config Files"));
	push(@pageContent, p("Choose a configuration file to edit."));
	
	push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','edit_config')"}));
	push(@pageContent, 
            popup_menu(
                -name=>"file",
                -values=>\@confFiles,
                -labels=>\%confFiles
            )
    );

	push(@pageContent, 
        submit({-onclick=>"whichPressed=this.name", -name=>"Edit"})
    );

	push(@pageContent, end_form);

	return join("", @pageContent);
}

sub listFiles
{
	# Files in ingex-config
	my $dirtoget="../";
	opendir(CONFIGS, $dirtoget) or die("Cannot open config directory");
	my @thefiles= readdir(CONFIGS);
	closedir(CONFIGS);
	
	my $f;
	my @fileList;
	
	foreach $f (@thefiles) {
		if(substr($f, -5) eq ".conf"){
			push(@fileList, $dirtoget.$f);
		}
	}
	
	# Module config files
	$dirtoget="../../ingex-modules/";
	opendir(MODULES, $dirtoget) or die("Cannot open modules directory");
	my @modules= readdir(MODULES);
	closedir(MODULES);
	
	my $module;
	foreach $module (@modules) {
		if(substr($module, -12) eq ".ingexmodule"){
			opendir(MODULE, $dirtoget.$module) or die("Cannot open module directory");
			@thefiles= readdir(MODULE);
			closedir(MODULE);
			foreach $f (@thefiles) {
				if(substr($f, -5) eq ".conf"){
					push(@fileList, $dirtoget.$module."/".$f);
				}
			}
		}
	}
	
	
	return @fileList;
}

END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


