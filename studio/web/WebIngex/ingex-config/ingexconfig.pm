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
 
package ingexconfig;

use strict;



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
        %ingexConfig
    );
}



####################################
#
# Exported package globals
#
####################################

our %ingexConfig;

load_config("/srv/www/cgi-bin/ingex-config/ingex.conf");
untaint();


####################################
#
# Load configuration
#
####################################

sub load_config
{
    my ($configFilename) = @_;
    
    my $configFile;
    open($configFile, "<", "$configFilename") or die "Failed to open config file: $!";
    
    while (my $line = <$configFile>) 
    {
        chomp($line);
        $line =~ s/#.*//;
        $line =~ s/^\s+//;
        $line =~ s/\s+$//;
        if (length $line > 0)
        {
            my ($key,$value) = split(/\s*=\s*/, $line, 2);
            $ingexConfig{$key} = $value;
        }
    }
}

sub untaint
{
	 # untaint values
        # TODO: do a better job with matching
        ($ingexConfig{"db_odbc_dsn"}) = ($ingexConfig{"db_odbc_dsn"} =~ /(.*)/)
            if ($ingexConfig{"db_odbc_dsn"});
        ($ingexConfig{"db_host"}) = ($ingexConfig{"db_host"} =~ /(.*)/)
            if ($ingexConfig{"db_host"});
        ($ingexConfig{"db_name"}) = ($ingexConfig{"db_name"} =~ /(.*)/)
            if ($ingexConfig{"db_name"});
        ($ingexConfig{"db_user"}) = ($ingexConfig{"db_user"} =~ /(.*)/)
            if ($ingexConfig{"db_user"});
        ($ingexConfig{"db_password"}) = ($ingexConfig{"db_password"} =~ /(.*)/)
            if ($ingexConfig{"db_password"});
}

1;

