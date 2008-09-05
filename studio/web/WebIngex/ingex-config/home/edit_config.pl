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
use ingexhtmlutil;

my $retval = "";

if (defined param('Cancel'))
{
    redirect_to_page("config.pl");
} 
elsif (defined param('Done'))
{	
	saveFile(param('filename'),param('content')) or return_error_page("Failed to save config file at ".param('filename').". The file may not exist, or may have incorrect permissions.");
}
elsif (defined param('file'))
{
	if(my $file = getFile(param('file'))){
		$retval = getContent($file,param('file')) or return_error_page("Failed to fill content for page");
	} else {
		return_error_page("Could not load config file.");
	}
} else {
	return_error_page("No config file specified.");
}

print "Content-Type: text/plain; charset=ISO-8859-1\n\n";
print $retval;
exit(0);

sub getFile
{
	(my $file) = @_;
	checkValid($file) or return_error_page("Attempting to edit a non-valid config file. Please check the filename.");
	my @data;
	my $line;
	my $filefound = 1;
	open (CONFFILE, '<'.$file) or $filefound = 0;
	if($filefound == 1) {
		while ($line = <CONFFILE>) {
			push(@data, $line);
		}
		close (CONFFILE);
		return join("",@data);
	}
	return 0;
}

sub saveFile
{
	my ($filename, $content) = @_;
	checkValid($filename) or return_error_page("Attempting to edit a non-valid config file. Please check the filename.");
	my $filefound = 1;
	if ($filename =~ /([a-zA-Z0-9._\-\/]+)/)
	{
		open (CONFFILE, '>'.$1) or $filefound = 0;
		if($filefound == 1) {
			print CONFFILE $content or return_error_page("Could not write config file.");
			close (CONFFILE);
			redirect_to_page("config.pl");
		}
	} else {
		return_error_page("Problem with filename.");
	}
	return 0;
}

sub getContent
{
    my ($file, $fileName) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Edit Configuration File"));
	push(@pageContent, h2($fileName));
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','edit_config')"}));

    push(@pageContent, hidden("filename", $fileName));

	push(@pageContent, textarea({-name=>"content", cols=>80, rows=>30, value=>$file}));

	push(@pageContent, 
		p(
			submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
			submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
		)
	);

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}

sub checkValid
{
	(my $fileToCheck) = @_;

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
	
	#if $fileToCheck is in @fileList, return 1, else return 0
	my $file;
	foreach $file (@fileList) {
		if ($file eq $fileToCheck) { return 1; }
	}
	return 0;
}
