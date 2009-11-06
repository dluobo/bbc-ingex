#!/bin/sh
# $Id: install.sh,v 1.4 2009/11/06 11:54:19 john_f Exp $
#
# Copyright (C) 2008-9  British Broadcasting Corporation
# All rights reserved
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

# One optional command line arg for the installation root directory
# which defaults to /srv/www
install_root=/srv/www
if [ -n "$1" ] ; then
	install_root=$1
fi

# Create the directories for CGI documents
mkdir -p $install_root/cgi-bin/ingex-modules || exit 1
mkdir -p $install_root/cgi-bin/ingex-config || exit 1
# Apply correct permissions to the directories
chmod a+rx $install_root/cgi-bin/ingex-modules || exit 1
chmod a+rx $install_root/cgi-bin/ingex-config || exit 1
# Copy the CGI files into these directories
rsync -arC ingex-modules $install_root/cgi-bin/ || exit 1
rsync -arC ingex-config $install_root/cgi-bin/ || exit 1
# Make all files read-only
find $install_root/cgi-bin/ingex-modules -type f -exec chmod 444 {} \; || exit 1
find $install_root/cgi-bin/ingex-config -type f -exec chmod 444 {} \; || exit 1
# Add exectue permissions to the Perl files
find $install_root/cgi-bin/ingex-modules -name "*.pl" -exec chmod 555 {} \;
find $install_root/cgi-bin/ingex-config -name "*.pl" -exec chmod 555 {} \;
# Make config files writable by the web server by changing their ownership (SuSE SPECIFIC!!!)
find $install_root/cgi-bin/ingex-modules -name "*.conf" -exec chown wwwrun:www {} \;
find $install_root/cgi-bin/ingex-modules -name "*.conf" -exec chmod 664 {} \;
find $install_root/cgi-bin/ingex-config -name "*.conf" -exec chown wwwrun:www {} \;
find $install_root/cgi-bin/ingex-config -name "*.conf" -exec chmod 664 {} \;

# Create the directory for the HTDocs
mkdir -p $install_root/htdocs/ingex || exit 1
# Apply correct permissions to the directory
chmod a+rx $install_root/htdocs/ingex || exit 1
# Copy the files into this directory
rsync -arC htdocs/* $install_root/htdocs/ingex || exit 1
# Put the favicon at the top level
cp htdocs/favicon.ico $install_root/htdocs || exit 1
# Make all files read-only
find $install_root/htdocs/ingex -type f -exec chmod 444 {} \; || exit 1
chmod 444 $install_root/htdocs/favicon.ico || exit 1

# tell the user what's happened
echo "All files have been copied to the appropriate places in $install_root/"
echo "If you haven't already done so, you will need to edit the WebIngex.conf file"
echo "to include your database details. E.g."
echo "  sudo vi $install_root/cgi-bin/ingex-config/WebIngex.conf"
echo "You must also define your Ingex system's nodes (what machines you have at"
echo "what IP addresses) on the Config page of WebIngex."
