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

/*
 * Injax : A simple javascript library of AJAX functions and similar
 *
 * CONFIG FILE
 * Simply adjust the values below to suit your setup. Each value that can be edited has a comment above to explain its meaning.
 */


injax = new Object({
	
	// The ID of the DOM element which holds your main content.
	// e.g. <div id='content'>My Content Here</div> would lead to you setting this to    contentPane : 'content',
	contentPane : 'contentPane',
	
	// This is the CSS class which controls the style of your content pane. This will be modufied by Insole when
	// you expand Insole, so that your content is not obscured by Insole. Therefore you *must* set this, and you
	// must not alter the style of your content pane in any other way (e.g. by inline styles in your HTML)
	contentClass : 'contentPane',
	
	// The ID of the DOM element which appears to indicate that your interface is loading content.
	// Set to false if you don't use such an element.
	loadingBox : 'loadingBox',
	
	// When loading new data, should the cursor be changed to a timer? true/false
	setCursor : true,
	
	// If you have a function to be run on page load, put its name here (not in quotes)
	// Otherwise, set to false
	customLoader : initIngex,
	
	// If you want to do processing before calling a loaderFunction, write a customCallLoader
	// Takes an argument which is the index of the loaderFunctions array to be called.
	// If unused, set to false
	customCallLoader : ingexCallLoader,
	
	// If you have a function to be run on page unload, put its name here (not in quotes)
	// Otherwise, set to false
	customOnUnload : ingexOnUnload,
	
	// Some Injax functions will turn a simple page name (e.g. 'index' or 'about') into a full URL (e.g. '/mySite/index.html')
	// Here you should set the prefix (e.g. '/mySite/') and suffix (e.g. '.html') to be used. 
	// If you do this, set customURLCreator : false
	// Alternatively, you can set your own function to do the translation. 
	// In this case, set customURLCreator as the name of your function (no quotes)
	customURLCreator : ingexPageToUrl,
	urlPrefix : '/ingex/',
	urlSuffix : '.html',
	
	// If you wish to have manual control over how pages are logged to the history, set your own custom add to history function.
	// This is necessary if you have a site structure more complicated than just a bunch of pages all in the same directory.
	// Othewise, set this to false.
	customAddToHistory : ingexAddToHistory,
	
	// If you wish to customise the format of your history string, you can replace the createHistoryString function with your own.
	// Simply enter its name here. (You must do this if you have a site structure more complicated than just a bunch of pages
	// all in the same directory)
	customHistoryString: false,
	
	// If you provide your own addToHistory function, you will also need to handle what happens when the URL changes, to decode
	// the data you stored in the URL. Specity your custom URL change function here, or otherwise set to false.
	customURLChange : ingexUrlChange,
	
	
	// Insole is Injax's INtegrated conSOLE.
	// It has its own configuration settings here...
	insole : {
		
		// The ID of the DOM element which contains Insole
		container : 'insole',
		
		// The ID of a DOM element to contain a "Show Insole"/"Hide Insole" link
		showHide : 'insoleShowHide',
		
		// Within the main Insole div (or other DOM element) should be another element to contain the main text output.
		// It must be a block level element (e.g. a div). Enter its ID here.
		outputDiv : 'consoleOutput',
		
		// When insole is asked to display an "alert" (a particularly improtant message), it pops up that message
		// in a "special message" element. Enter the ID of that DOM element here. (Must be block-level element, e.g. div)
		specialDiv : 'insoleSpecial',
		
		// An "alert" will be shown for a finite amount of time, then hidden. This value specified the time
		// (in milliseconds) to show the message for.
		alertTime : 8000, 
		
		// Place new items at the top of the console rather than the bottom - true/false
		newItemsAtTop : true,
		
		// If a sysLog is available (in Safari or in Firefox using the FireBug extension), also send messages there - true/false
		logToSysLog : false
	},

	// Some state variables used by Injax. Do not edit.
	requestsWaiting : 0,
	visibleElement : ''
});