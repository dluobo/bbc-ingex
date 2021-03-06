#!/usr/bin/perl -wT

#
# $Id: createproxydf.pl,v 1.1 2007/09/11 14:08:46 stuart_hc Exp $
#
# 
#
# Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
 
use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use config;
use prodautodb;
use datautil;
use htmlutil;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or return_error_page("failed to connect to database");


    
my $errorMessage;

if (defined param("Cancel"))
{
    redirect_to_page("proxydf.pl");
}
elsif (defined param("Create"))
{
    if (!($errorMessage = validate_params()))
    {
        my $pxdf = create_proxy_def(trim(param("name")), param("proxytype"))
            or return_create_page("failed to create proxy definition");

        my $pxdfId = save_proxy_def($dbh, $pxdf) 
            or return_create_page("failed to save proxy definition to database: $prodautodb::errstr");
        
        redirect_to_page("editproxydf.pl?id=$pxdfId");
    }
}

my $pxts = load_proxy_types($dbh) 
    or return_error_page("failed to load proxy types from database: $prodautodb::errstr");
    
return_create_page($pxts, $errorMessage);



sub validate_params
{
    return "Error: Empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: Missing proxy type identifier" if (!defined param("proxytype"));
    
    return undef;
}

sub return_create_page
{
    my ($pxts, $errorMessage) = @_;
    
    my $page = construct_page(get_create_content($pxts, $errorMessage)) or
        return_error_page("failed to fill in content for create proxy definition page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_create_content
{
    my ($pxts, $message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Create new proxy definition"));

    if (defined $message)
    {
        push(@pageContent, p({-class=>"error"}, $message));
    }
    
    push(@pageContent, start_form({-method=>"POST", -action=>"createproxydf.pl"}));

    push(@pageContent, p("Name", textfield("name")));

    push(@pageContent, p("Type", get_proxy_types_popup("proxytype", $pxts)));

    push(@pageContent, submit("Create"), span(" "), submit("Cancel"));

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


