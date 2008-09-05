# Copyright (C) 2008  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
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
 
package materialconfig;

use strict;
use lib '../../ingex-config/';
use ingexconfig;



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
        &get_all_avid_aaf_export_dirs
        &get_avid_aaf_export_dir
        &get_all_directors_cut_dbs
        &get_directors_cut_db
    );
}



materialconfig::load_config();


####################################
#
# Load configuration
#
####################################

sub load_config
{
    ingexconfig::load_config("/srv/www/cgi-bin/ingex-modules/Material.ingexmodule/material.conf");

	($ingexConfig{"create_aaf_dir"}) = ($ingexConfig{"create_aaf_dir"} =~ /(.*)/)
	                if ($ingexConfig{"create_aaf_dir"});
	            ($ingexConfig{"avid_aaf_export_dir"}) = ($ingexConfig{"avid_aaf_export_dir"} =~ /(.*)/)
	                if ($ingexConfig{"avid_aaf_export_dir"});
	            ($ingexConfig{"avid_aaf_prefix"}) = ($ingexConfig{"avid_aaf_prefix"} =~ /(.*)/)
	                if ($ingexConfig{"avid_aaf_prefix"});
	            ($ingexConfig{"directors_cut_db"}) = ($ingexConfig{"directors_cut_db"} =~ /(.*)/)
	                if ($ingexConfig{"directors_cut_db"});

	            ($ingexConfig{"default_material_rows"}) = ($ingexConfig{"default_material_rows"} =~ /(.*)/)
	                if ($ingexConfig{"default_material_rows"});
	            ($ingexConfig{"max_material_rows"}) = ($ingexConfig{"max_material_rows"} =~ /(.*)/)
	                if ($ingexConfig{"max_material_rows"});
} 

    
####################################
#
# Load configuration
#
####################################

sub get_all_avid_aaf_export_dirs
{
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"avid_aaf_export_dir"});

    my @exportDirs;
    foreach my $pair (@pairs)
    {
        my ($name, $dir) = split(/\s*\|\s*/, $pair);
        push(@exportDirs, [$name, $dir]);
    }
    
    return @exportDirs;
}

sub get_avid_aaf_export_dir
{
    my ($targetName) = @_;
    
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"avid_aaf_export_dir"});
    
    foreach my $pair (@pairs)
    {
        my ($name, $dir) = split(/\s*\|\s*/, $pair);
        if ($name eq $targetName)
        {
            return (defined $dir) ? $dir : "";
        }
    }
    
    return undef;
}


sub get_all_directors_cut_dbs
{
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"directors_cut_db"});

    my @dcDBs;
    foreach my $pair (@pairs)
    {
        my ($name, $filename) = split(/\s*\|\s*/, $pair);
        push(@dcDBs, [$name, $filename]);
    }
    
    return @dcDBs;
}

sub get_directors_cut_db
{
    my ($targetName) = @_;
    
    my @pairs = split(/\s*\,\s*/, $ingexConfig{"directors_cut_db"});
    
    foreach my $pair (@pairs)
    {
        my ($name, $filename) = split(/\s*\|\s*/, $pair);
        if ($name eq $targetName)
        {
            return (defined $filename) ? $filename : "";
        }
    }
    
    return undef;
}


1;

