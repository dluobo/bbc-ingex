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

Introduction
------------
This is some guidance on how the code for this module works. It follows on from Develop.txt, found in the WebIngex directory when you check out from CVS. For installation instructions etc, see README.txt in the WebIngex directory.

Software Structure
------------------
As mentioned in Develop.txt, the Status module has its own sub-structure of "monitors", allowing you to easily add extra data monitors. In the monitors/ directory, look at Template.pl for a guide on how to make your own. Your monitor will consist of a Perl file called monitorName.ingexmonitor.pl plus Javascript (monitorName.js) and optionally CSS (monitorName.css) files. They should all be placed in the monitors/ directory.

When you define your monitor, you will specify its "fields" and their types. Unless re-using types defined elsewhere, you will need to register a "TypeHandler" for each type. This is a javascript function which takes the data from your monitor, and renders it to the screen. The best thing here is definitely to look at the existing examples, but your code will look like this:
	function Status_display_myFieldType (instanceData) {
		// insert code here
	}
	typeHandlers.myFieldType = Status_display_myFieldType;

There are 2 query methods available for a monitor. It is possible to have your monitor's Perl file calculate/find/query the data and return it directly. Alternatively, you can specify an http monitor, whereby the Status module will query the data directly by making an http request to elsewhere on your network. In the latter case, your monitor's Perl file is simply a descriptor for the monitor's properties. In either case, refer to Template.pl for details.

All data communication between the web client and the monitors is in the form of JSON (JavaScript Object Notation). If you are not familiar with JSON, I recommend looking it up now at http://www.json.org