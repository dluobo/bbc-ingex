/*
 * Copyright (C) 2008  British Broadcasting Corporation
 * Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

README FOR INGEX MODULE TEMPLATE

This is a sample Ingex module to show you how to construct your own. It contains:
	index.pl - [required] a welcome page for your module
	menu.inc.html - [required] the menu code for your module - an HTML unordered list usually
	page1.pl - [optional] a sample page for your module
	javascript.js - [optional] any javascript functions you wish to define for use in your module
	styles.css - [optional] any css styles you wish to define for use in your module

Your own module can include as many pages as you like, plus any Perl modules you like. The latter can be useful for adding database handling functions, config file handling and more, with subroutines available to all your module's pages. Examples of such techniques can be seen in the Material and Setup modules.

To see this module in action, simply rename it (i.e. the directory which contains the above files) Template.ingexmodule
To disable it, rename it back to just Template.