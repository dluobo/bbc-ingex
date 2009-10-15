#!/usr/bin/perl -wT

# Copyright (C) 2009  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
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
#


#
# Delete test - demonstrates how materials packages can be deleted
#

use strict;
use warnings;

use lib "../../ingex-config";
use lib "../../ingex-modules/Material.ingexmodule";

use db;
use materialconfig;
use fileActions;

use JSON::XS;
use CGI qw(:standard);

my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();
   	
my $pkgid = '1';

print header;
my $result = delete_pkg($pkgid, $dbh);

print "ok";