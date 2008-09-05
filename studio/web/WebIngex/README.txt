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

Installation
------------

Requirements:
* Apache2 web server, with static html documents in /srv/www/htdocs and cgi in /srv/www/cgi-bin
* Perl CGI, CGI::Pretty, DBI, PostgreSQL DBD modules

CGI and CGI::Pretty should already be present as part of perl. perl-DBI and perl-DBD-Pg can be installed using YaST.

1) edit the configuration file "ingex-config/ingex.conf" to match your system setup, including the IP address of your database server.
2) run "sudo ./install.sh" to install the files.

Updating
--------

An extra script is provided to update to the latest copy from CVS without overwriting your custom config files. Instead of running install.sh, use update.sh. This will overwrite all files in your installation except those with a .conf extension. 

Usage
-----

Open your web browser to the page http://<host name>/ingex/

To view a single module, without monitoring or access to other modules, simply head to http://<host name>/ingex/#_<ModuleName>
e.g. http://localhost/ingex/#_Setup

Development
-----------

An introduction to the software structure etc is provided in Develop.txt. It may be useful as an introduction if you wish to make any changes to this software.

