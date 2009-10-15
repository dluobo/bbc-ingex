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
# Materials help page
#

use strict;

use lib "../../ingex-config";
use lib ".";

use ingexhtmlutil;


use CGI::Pretty qw(:standard);

print header;
my $content = get_content();
print $content;
exit(0);

sub get_content{
	# html content for main page body
	my $content = <<ENDHTML;
		<html>
			<h1>Materials Page Guide</h1>
			<p>The materials page allows encoded video materials to be browsed and exported in either Avid or Final Cut format. There are two main parts to the page:</p>
			<br>
			
			<li><b>Materials Browser:</b> Seen on top. This contains all original materials which are held on the INGEX system.</li>
			<li><b>Materials Selection:</b> This is where to put any materials which need to be exported.</li>
			<br>
			
			<h3>Browsing Materials</h3>
			<p>Recorded materials are found in the <i>materials browser</i> and organised into expanding categories. Initially, the browser shows a list of project names. Each of these can be expanded by clicking the arrow to the left of the name. Within each project name is a set of recording dates and within each date, a set of recording times. Expanding these times shows the original source material items. Selecting an individual material item reveals the tracks it contains in the yellow 'Source Details' panel to the right. To download a track locally in MXF format, click the required track.</p>
			
			<br>
			<h3>Selecting Materials</h3>
			<p>Either groups of materials (for example by selecting a particular date) or individual material items can be selected. To select multiple items, either ctrl-click or shift-click may be used. In order to include these materials for export, drag the selected items from the box onto the white <i>Materials Selection</i> window below.</p>
			
			<br>
			<h3>Filter Options</h3>
			<p>In order to restrict the range of materials displayed, it is possible to filter them using various properties, such as project name, material text, resolution and time. Complete the details and click 'Update' to refine the search.</p>
			
			<br>
			<h3>Exporting Materials</h3>
			<p>Materials in the <i>Materials Selection</i> can be packaged together and exported in either Avid or Final Cut formats. To begin exporting, ensure the correct materials are included in the selection, check the export options (see below) and click 'Create'. A status box will be shown while this process takes place. Please note that it can take several minutes. Once complete, the URLs for the exported package(s) will be shown. Click each of these to download and save locally.</p>
			
			<br>
			<h3>Export Options</h3>
			<p>The options set how the exported material will be packaged. Checking the 'Save these options as global defaults' box will make these the default settings on your local computer.</p>
			
			<br>
			<h3>Deleting Materials</h3>
			<p>To remove materials from the <i>Materials Selection</i>, select the materials you wish to discard and click the 'delete' link to the right hand side of the window.</p>
			<p>To remove original materials from the <i>Materials Browser</i>, select the materials you wish to discard and click the 'delete' link to the right hand side of the window. Please note these materials will be permanently deleted from the database and cannot be retrieved. This will only delete the database entries and not the MXF video files associated with the items.</p>
			
			<br>
			<br>
		</html>
ENDHTML

	return $content;
}