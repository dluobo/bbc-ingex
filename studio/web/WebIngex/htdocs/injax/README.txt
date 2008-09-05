/*
 * Copyright (C) 2008  British Broadcasting Corporation
 * Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
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

==============
WHAT IS INJAX?
==============

Injax is a very simple Javascript library which came from the AJAX components of the Ingex project, hence Injax. It provides:

== A Simple AJAX Wrapper ==
	* Create XML HTTP Request objects
	* Simple standard interfaces for updating the contents of DOM elements with the results of AJAX requests
	* Execute javascript functions on receipt of remote data

== More Complex AJAX Features ==
	* Submit (arbitrarily complex) forms via AJAX and display the results

== Application State Management ==
	* Provide back/forward/history/bookmarking capabilities through the use of RSH (see below)

== DOM Manipulation ==
	* Add some syntactic sugar to simple operations like locating DOM elements by their ID and storing DOM IDs
	* Show and hide DOM elements

== Logging, Debugging and Feedback ==
	* Includes "Insole", an integrated console
		- provides message logging straight to the screen in a customisable manner
		- can be shown or hidden at any time
		- provides multiple message types including alerts which display immediately even if console is hidden

Please note that Injax was written for the specific purpose of Ingex's web interface. I have done my best to make it useful to the outside world by packaging the generic functions up in a helpful way. However this is not a full-featured library like Prototype, and is nowhere near as sophisticated as things like SproutCore. It is what it is, and might be useful for a variety of fairly simple AJAX sites. If it's helpful to you, great (and I'd love to hear what you've used it for - rdepom@users.sourceforge.net). If it's not up to what you need, you have 2 options:
	* Change it - it's open source, feel free to make modifications. Again, if you add anything good, I'd love to hear about it.
	* Look elsewhere - there are plenty of great Javascript libraries around.

===========================
LICENSE & INCLUDED SOFTWARE
===========================

Injax is free software, licensed under the LGPL license. Included with it are 2 additional libraries, which are used by Injax itself, but which provide features which may be useful to you too. Both libraries are free software, but their licenses are different to Injax's LGPL license. Details are in the files themselves. The libraries are:

	* Really Simple History (RSH) from http://code.google.com/p/reallysimplehistory/
		- Files used by Injax: rsh.js blank.html
		- N.B. This version has been modified for Injax - see the notes at the top of rsh.js
		- The full distribution of RSH, including the unmodified version and the README and LICENSE files, are included too
			(in the directory RSH0.6FINAL).
		
	* The JSON2 parser from http://www.JSON.org/json2.js
		- One file only: json2.js

=====
USAGE
=====

To use Injax, simply copy the appropriate files into your website directory. You will need:
	injax.js
	injax.css
	injax_config.js
	json2.js
	rsh.js
	blank.html (used by RSH)

You will need to configure Injax by editing injax_config.js - the comments in that file should explain all you need to know.

You will also need to include the relevant Javascript and css files into your HTML page(s), and if you wish to use Insole and/or the "loadingBox" function of Injax, you will need to add the relevant divs (or other DOM elements) to your page. index.html is a sample page to show you how to do these things. You will want to adjust the CSS in oldIE.css to make insole work with your page in IE6 - it cannot be position:fixed so you will need to position it to fit your layout. (You may additionally wish to customise injax.css to fit your site's style.)

The comments in injax.js explain the functions that Injax provides and how to use them.

=======
SUPPORT
=======

This software is provided as-is, without warranty or support. However, I will do my best to answer any good-natured questions sent to me: rdepom@users.sourceforge.net
