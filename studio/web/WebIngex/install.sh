#!/bin/sh

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

# very simple installation

# Create the directories for CGI documents
mkdir -p /srv/www/cgi-bin/ingex-modules
mkdir -p /srv/www/cgi-bin/ingex-config
# Apply correct permissions to the directories
chmod a+rx /srv/www/cgi-bin/ingex-modules
chmod a+rx /srv/www/cgi-bin/ingex-config
# Copy the CGI files into these directories
rsync -arC ingex-modules /srv/www/cgi-bin/
rsync -arC ingex-config /srv/www/cgi-bin/
# Make all files read-only
find /srv/www/cgi-bin/ingex-modules -type f -exec chmod 444 {} \;
find /srv/www/cgi-bin/ingex-config -type f -exec chmod 444 {} \;
# Add exectue permissions to the Perl files
find /srv/www/cgi-bin/ingex-modules -name "*.pl" -exec chmod 555 {} \;
find /srv/www/cgi-bin/ingex-config -name "*.pl" -exec chmod 555 {} \;
# Make config files writable by the web server by changing their ownership (SuSE SPECIFIC!!!)
find /srv/www/cgi-bin/ingex-modules -name "*.conf" -exec chown wwwrun:www {} \;
find /srv/www/cgi-bin/ingex-modules -name "*.conf" -exec chmod 664 {} \;
find /srv/www/cgi-bin/ingex-config -name "*.conf" -exec chown wwwrun:www {} \;
find /srv/www/cgi-bin/ingex-config -name "*.conf" -exec chmod 664 {} \;

# Create the directory for the HTDocs
mkdir -p /srv/www/htdocs/ingex
# Apply correct permissions to the directory
chmod a+rx /srv/www/htdocs/ingex
# Copy the files into this directory
rsync -arC htdocs/* /srv/www/htdocs/ingex
# Put the favicon at the top level
cp htdocs/favicon.ico /srv/www/htdocs
# Make all files read-only
find /srv/www/htdocs/ingex -type f -exec chmod 444 {} \;
chmod 444 /srv/www/htdocs/favicon.ico

# tell the user what's happened
echo "All files have been copied to the appropriate places in /srv/www/"
echo "You will need to edit the ingex.conf file to include your database details. e.g. sudo pico /srv/www/cgi-bin/ingex-config/ingex.conf"
echo "You must also define your Ingex system's nodes (what machines you have at what IP addresses) on the Config page of WebIngex."

