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
 * JavaScript functions for use in WebIngex
 * For use in conjunction with the more generic Injax library
 */ 

// ----- OPTIONS -----
// In timecode display, show frame number?
var showFrames = false;
// How often to increment the timecodes? [A number of frames - e.g. 3 means update every 3 frames, i.e. every 120ms]
var tcFreq = 3; // every 3 frames
// --------------------

// Some state variables for the system
var moduleJSLoaded = false;
var currentTab = '';
var singleTab = false;
var viewingConfig = false;

/// A convenience function to automate the processing of updating the ContentPane
/// with the results of an HTTP Request - assumes you're using WebIngex modules and
/// want the results in an element specified by injax.contentPane. Unlike getContentWithVars,
/// getContent has no facility for adding GET data.
/// @param pageName the filename of the page to get, excluding the extension
/// @param fullPathSupplied set true if you supply a full path to the file, otherwise just supply the filename without the extension
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed 
/// @param logToHistory if true, add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param allowContinue If false, the call is synchronus (blocking), if true (or unspecified) the request is asynchronus
function getContent(pageName, fullPathSupplied, loaderFunction, historyString, allowContinue)
{
	getContentWithVars(pageName, null, fullPathSupplied, loaderFunction, historyString, allowContinue)
}

/// A convenience function to automate the processing of updating the ContentPane
/// with the results of an HTTP Request - assumes you're using WebIngex modules and
/// want the results in an element specified by injax.contentPane.
/// @see getElementWithVars
/// @param pageName the filename of the page to get, excluding the extension
/// @param vars a URL-encoded string of variables as GET data
/// @param fullPathSupplied set true if you supply a full path to the file, otherwise just supply the filename without the extension
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed 
/// @param logToHistory if true, add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param allowContinue if false, the call is synchronus/blocking, if true or unspecified, the request is asynchronus
function getContentWithVars(pageName, vars, fullPathSupplied, loaderFunction, historyString, allowContinue)
{
	if (typeof monitors != "undefined") monitors.stopDisplay();
	
	if (isDefault(fullPathSupplied)) fullPathSupplied = false;
	if (isDefault(historyString)) historyString = true;
	
	var url;
	if(fullPathSupplied){
		url = pageName;
	} else {
		url = pageToURL(pageName);
	}
	if(historyString == true){
		if(fullPathSupplied){
			historyString = pageName+"::"+fullPathSupplied;
		} else {
			historyString = pageName;
		}
	}
	getElementWithVars(injax.contentPane,url,vars,null,loaderFunction,historyString,allowContinue);
}

/// Load a tab's contents into the contentPane, get its menu and update the selected tab graphics.
/// Assumes you're using WebIngex modules.
/// @param tabName the name of the tab to load
/// @param optionNotTab set to true if loading an options page not a tab e.g. config, help etc
/// @param logToHistory if true, add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param getContent if true, also load the index page of the tab - if set false, only the tabs and menu will be updated
/// @param postStr a POST-encoded string of variables - setting this non-null automatically selects a POST-type request
function getTab(tabName, optionNotTab, logToHistory, getContent, postStr) 
{
	if (isDefault(optionNotTab)) optionNotTab = false;
	if (isDefault(getContent)) getContent = true;
	if (isDefault(postStr)) postStr = null;
	
	
	//if current module (i.e. the one we're leaving) is Status, stop trying to display status updates
	if(currentTab == "Status") {
		monitors.stopDisplay();
	}
	
	//if current tab is config, re-create the monitors object to take into account any changes we just made
	if(viewingConfig) {
		viewingConfig = false;
		if(typeof monitors != "undefined"){
			insole.log("Re-creating the monitors object to take into account any changes you may have just made.");
			monitors = new ingexMonitors(false);
		}
	}
	if(tabName == "config") viewingConfig = true;
	
	//update the tabs
	if(currentTab!='home' && currentTab!=''){
		if($(currentTab+"_tab")) $(currentTab+"_tab").className =  "tab";
	}
	if(optionNotTab){
		currentTab = 'home';
	} else {
		if($(tabName+"_tab")) $(tabName+"_tab").className =  "tabSelected";
		currentTab = tabName;
	}
	
	if(typeof getContent == "string") {
		if(optionNotTab){
			addToHistory(tabName);
		} else {
			addToHistory('index');
		}
		injax.contentPane.$().innerHTML = getContent;
	} else if(getContent == true) {
		showLoading(injax.contentPane,true);
		//get the content
		var urlToGet="";
		var historyString = false;
		if(optionNotTab){
			urlToGet="/cgi-bin/ingex-config/home/"+tabName+".pl";
			historyString = tabName;
		} else {
			urlToGet="/cgi-bin/ingex-modules/"+tabName+".ingexmodule/index.pl";
			historyString = 'index';
		}
		if(logToHistory == false){
			historyString = false;
		}
		getElementWithVars(injax.contentPane,urlToGet,null,postStr,tabName,historyString,true);
	}

	//get the menu
	if((optionNotTab && tabName == "index") || tabName == "Logging" || tabName == "Material"){
		$("menu").style.display = "none";
		$("contentPane").style.marginLeft = "";
	} else if (optionNotTab) {
		menuFilename = "../ingex-config/home/menu.inc.html";
		$("menu").style.display = "block";
		$("contentPane").style.marginLeft = "200px";
		getElementWithVars("menu","/cgi-bin/ingex-config/getMenu.pl","filename="+menuFilename,null,null,false,true);
	} else {
		menuFilename = "../ingex-modules/"+tabName+".ingexmodule/menu.inc.html";
		$("menu").style.display = "block";
		$("contentPane").style.marginLeft = "200px";
		getElementWithVars("menu","/cgi-bin/ingex-config/getMenu.pl","filename="+menuFilename,null,null,false,true);
	}
}

/// Load the tabs of the WebIngex interface.
/// @param the module to load (if loading only one module's tab), or false for all modules
function drawTabs(module)
{
	if (isDefault(module)) module = false;
	if(module){
		getElementWithVars("tabs","/cgi-bin/ingex-config/tabs.pl","tab="+module,null,null,false,false);
	} else {
		getElementWithVars("tabs","/cgi-bin/ingex-config/tabs.pl",null,null,null,false,false);
	}
}

/// Load the javascript for a single WebIngex module or all of them.
/// @param module The module whose javascript should be loaded, or false
function loadModuleJS (module) {
	if (isDefault(module)) module = false;	
	var fileref=document.createElement('script');
	fileref.setAttribute("type","text/javascript");
	if(module){
		fileref.setAttribute("src", '/cgi-bin/ingex-config/module_js.pl?tab='+module);
	} else {
		fileref.setAttribute("src", '/cgi-bin/ingex-config/module_js.pl');
	}
	fileref.setAttribute("id", 'module_js');
	if (typeof fileref!="undefined"){
		document.getElementsByTagName("head")[0].appendChild(fileref);
	}
}

/// Load the CSS for a single WebIngex module or all of them.
/// @param module The module whose CSS should be loaded, or false
function loadModuleCSS (module) {
	if (isDefault(module)) module = false;
	var fileref=document.createElement('link');
	fileref.setAttribute("type","text/css");
	if(module){
		fileref.setAttribute("href", '/cgi-bin/ingex-config/module_css.pl?tab='+module);
	} else {
		fileref.setAttribute("href", '/cgi-bin/ingex-config/module_css.pl');
	}
	fileref.setAttribute("id", 'module_css');
	fileref.setAttribute("media", 'screen');
	fileref.setAttribute("rel", 'stylesheet');
	fileref.setAttribute("rev", 'stylesheet');
	if (typeof fileref!="undefined"){
		document.getElementsByTagName("head")[0].appendChild(fileref);
	}
}

/// Look at which module to display, from the URL
/// @return the module name, or false for all modules
function whichModule () {
	var query = window.location.hash;
	// Skip the leading #
	if (query.substring(0, 1) == '#') {
		query = query.substring(1);
	}
	//Split the location string into:
	//	Tab
	//	Page
	//	FullPathSpecified
	//	LoaderFunction
	var chunks = query.split("::");
	
	if (chunks[0].substring(0, 1) == '_') {
		// If the Tab starts with an underscore, we want a single tab only
		singleTab = true;
		// Set the logo link to reload with only a single tab
		$('ingexLogo').href = "/ingex/index.html#_"+chunks[0].substring(1);
		// return the tab name, minus the underscore
		return chunks[0].substring(1);
	}
	
	// Otherwise we want all tabs, so return false
	return false;
}

/// An initialise function, which is called by Injax's pageLoad()
/// Determines which module to display - or all of them - and loads the css, javascript and tabs
var javascriptLoadAttempts = 0;
function initIngex () {
	window.loading = new Image();
	window.loading.src = "/ingex/img/loading.gif";
	var Module=whichModule();
	loadModuleJS(Module);
	loadModuleCSS(Module);
	drawTabs(Module);
	if(!singleTab) {
		$('oshIcon').style.display = "inline";
		$('options').style.display = "block";
		$('numInputs').style.display = "block";
	}
	if (typeof window.monitors != "undefined") window.monitors.startAll();
	// Specify function to draw system info on home page
	loaderFunctions.index = function() {
		checkJSLoaded(function(){loadIngexMonitors(displaySystemOverview);});
	}
}

function loadIngexMonitors (callback) {
	if (typeof monitors == "undefined") {
		// Create our monitors object
		try {
			monitors = new ingexMonitors(false);
		} catch(e) {
			insole.alert("Could not load monitors. Is the database available? Error thrown was: "+e.message);
			if(injax.setCursor) document.body.style.cursor='default';
			if(injax.loadingBox && injax.loadingBox.$()) injax.loadingBox.$().style.display='none';
			var si;
			if(si = $('systemInfo')) si.innerHTML = "<span class = 'error'>Error: Could not connect to database and load monitors</span>";
		}
	}
	if(callback) callback();
}

function checkJSLoaded (callback) {
	if (isDefault(callback)) callback = false;
	if(moduleJSLoaded) {
		if(callback) callback();
	} else {
		javascriptLoadAttempts++;
		insole.warn("Had to wait for dynamic javascript to load before monitors object could be created. Attempts made: "+javascriptLoadAttempts);
		if(javascriptLoadAttempts < 10) {
			setTimeout(function(){checkJSLoaded(callback);},500);
		} else {
			insole.alert("Repeated attempts to create ingexMonitors object failed. Likely cause: Module Javascript loading error.");
			alert("Could not load all the javascript code and initiate monitoring. Try re-loading the page, and if this error recurrs please contact your system administrator.");
		}
	}
}

var callLoaderAttempts = 0;
function ingexCallLoader (loader) {
	if(moduleJSLoaded) {
		callLoaderAttempts = 0;
		if(typeof loaderFunctions[loader] != "undefined") {
			loaderFunctions[loader]();
		}
	} else {
		if(callLoaderAttempts < 10) {
			callLoaderAttempts++;
			setTimeout(function(){ingexCallLoader(loader);},500);
		} else {
			insole.alert("Repeated attempts to call a loader function failed. Likely cause: Module Javascript loading error.");
		}
	}
}

/// Display some information about your Ingex setup and its availability
function displaySystemOverview () {
	var output = "<table class='nodeTable'>";
	output += "<tr><td><h4>Database</h4><p>Ingex database : <span id='db_status'></span></p></td>";
	var numOnLine = 1;
	for (var node in monitors.nodes) {
		var nodeBoxID = "nodeBox_"+node;
		if(numOnLine == 0) output += "<tr>";
		output += "<td id='"+nodeBoxID+"'><h4>"+monitors.nodes[node].nodeType+" : "+node+" ("+monitors.nodes[node].ip+")</h4>";
		for (var monitor in monitors.m) {
			for (var t in monitors.m[monitor].appliesTo) {
				var type = monitors.m[monitor].appliesTo[t]
				if (type == monitors.nodes[node].nodeType) {
					var id = "nodeAvailability_"+node+"_"+monitor;
					output += "<p>Monitoring of '"+monitors.m[monitor].friendlyName+"' : <span id='"+id+"'></span></p>";
				}
			}
		}
		output += "</td>"
		numOnLine++;
		if (numOnLine == 2) {
			numOnLine = 0;
			output += "</tr>";
		}
	}
	if (numOnLine == 1) {
		output += "<td style='background-color: transparent; border: none;'></td></tr>";
	}
	output += "</table><p class='small center'><a href='javascript:checkAllAvailability();'>[Refresh Status]</a></p>";
	$('systemInfo').innerHTML = output;
	checkAllAvailability();
}

/// Check the availability of each node/monitor and display it
function checkAllAvailability () {
	showLoading('db_status');
	checkDB('db_status');
	for (var node in monitors.nodes) {
		for (var monitor in monitors.m) {
			for (var t in monitors.m[monitor].appliesTo) {
				var type = monitors.m[monitor].appliesTo[t]
				if (type == monitors.nodes[node].nodeType) {
					var id = "nodeAvailability_"+node+"_"+monitor;
					showLoading(id);
					monitors.m[monitor].checkAvailability(node,id);
				}
			}
		}
	}
}

/// Check the availability of the databse connection
/// @param display The ID of the DOM element in which to render the result
var DBajaxtimeout;
function checkDB (display) {
	var xmlHttp = getxmlHttp();
	
	function stateChanged(display) {
		if (xmlHttp.readyState==4)
		{
			if (DBajaxtimeout) {
				clearTimeout(DBajaxtimeout);
				DBajaxtimeout = false;
			}
			try {
				var info = JSON.parse(xmlHttp.responseText);
				if(info.dbError == false) {
					$(display).innerHTML = "Available.";
				} else {
					$(display).innerHTML = "<span class='error'>Could not contact database</span>.";
				}
			} catch (e) {
				$(display).innerHTML = "<span class='error'>Received invalid response when checking database</span>.";
				insole.alert("Received invalid response when checking database availability (JSON parse error)");
				insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
			}
		}
	}

	function DBajaxTimedout(display) {
		xmlHttp.abort();
		$(display).innerHTML = "<span class='error'>Unavailable</span>.";
		clearTimeout(DBajaxtimeout);
		DBajaxtimeout = false;
		insole.error("AJAX timeout occurred when querying trying to perform database check.");
	};

	if (DBajaxtimeout) {
		clearTimeout(DBajaxtimeout);
		DBajaxtimeout = false;
	}
	
	xmlHttp.open("GET","/cgi-bin/ingex-config/check_db.pl",true);
	xmlHttp.onreadystatechange = function (){stateChanged(display);};
	DBajaxtimeout = setTimeout(function(){DBajaxTimedout(display)},5000);
	xmlHttp.send(null);
}

/// Act on a change to the location. see RSH - http://code.google.com/p/reallysimplehistory/
/// @param newLocation the new location to load
/// @historyData JSON data to add to the history store
function ingexUrlChange(newLocation, historyData) {
	var loader = null;
	var fullPathSupplied = false;
	var url;
	
	//No specified location means the home page
	if(newLocation == ''){
		newLocation = 'home::index';
	}
	
	//Split the location string into:
	//	Tab
	//	Page
	//	FullPathSpecified
	//	LoaderFunction
	var chunks = newLocation.split("::");
	
	// The tab name will have an underscore if we are viewing only a single module
	// Strip this out for processing
	if (chunks[0].substring(0, 1) == '_') {
		chunks[0] = chunks[0].substring(1);
	}

	if(chunks[1] == "" || typeof chunks[1] == 'undefined'){
		chunks[1] = 'index';
	}
	
	//if a form has been submitted, we want to divert to its submission results page
	//(with no form data - i.e. the equivelant of clicking cancel)
	var postStr = null;
	if(chunks[3] == 'submit'){
		postStr = "Cancel=Cancel&Cancel1=Cancel1&Cancel2=Cancel2";
	}

	//Are we in the 'home' tab? (i.e. no tab!)
	if(chunks[0] == 'home'){
		//If home, update the menu, deselect all tabs and retrieve the page, specifying optionNotTab
		getTab(chunks[1],true,false,true,postStr);
	} else {
		//Do we need to update the selected tab and the menu?
		if(chunks[0] != currentTab){
			getTab(chunks[0],false,false,false);
			//If we're loading a new tab, we'll want to use its name as a loaderFunction
			loader = chunks[0];
		} else if (chunks[1] == "index"){
			// Want to run the tab loader if we're returning to the index page
			loader = chunks[0];
		}
		//is it a full path or just a page name?
		if(chunks[2] != 'true'){
			url = '/cgi-bin/ingex-modules/'+chunks[0]+'.ingexmodule/'+chunks[1]+'.pl';
		}
		if(chunks[3]) {
			loader = chunks[3];
		}
		//Load the content pane
		getElementWithVars('contentPane',url,null,postStr,loader,false,true);
	}
}

/// Custom override for Injax's similar function
/// Formats the current application state for saving in the URL and calls the RSH dhtml.add() function
/// @param logEntry the text to add to the URL (excluding the hash sign itself)
/// @param isFormSubmission set true if submitting a form, ::submit will be added to url, allowing handling on refreshing the page
function ingexAddToHistory (logEntry,isFormSubmission) {
	if (isDefault(isFormSubmission)) isFormSubmission = false;
	if (isFormSubmission) logEntry += "::::submit";
	if (singleTab) {
		if(logEntry) dhtmlHistory.add("_"+currentTab+"::"+logEntry);
	} else {
		if(logEntry) dhtmlHistory.add(currentTab+"::"+logEntry);
	}
}

/// Ingex's own pageToURL function - to override Injax's one
/// Simply takes a pagename and adds the directory to the start and the extension to the end
/// Assumes standard WebIngex file locations and that the page is a .pl file
/// @param pageName the page's filename minus the extension
/// @return the URL
var ingexPageToUrl = function(pageName) {
	if(currentTab!='home') {
		return "/cgi-bin/ingex-modules/"+currentTab+".ingexmodule/"+pageName+".pl";
	} else {
		return "/cgi-bin/ingex-config/home/"+pageName+".pl";
	}
}

/// OnUnLoad function (used by Injax) to clear monitor updates
var ingexOnUnload = function() {
	if (typeof window.monitors != "undefined") window.monitors.clearAllPolls();
}

/// Show a loading image in a DOM element
/// @param display the ID of the element to display the image in
/// @param center set true to centre the image
function showLoading (display, center) {
	if (isDefault(center)) center = false;
	if($(display)) {
		if(center) {
			$(display).innerHTML = "<div class='center'><img class='loadingImage' id='loadingIcon_"+display+"' /></div>";
		} else {
			$(display).innerHTML = "<img class='loadingImage' id='loadingIcon_"+display+"' />";
		}
		if(typeof window.loading != "undefined" && $("loadingIcon_"+display) ) $("loadingIcon_"+display).src = window.loading.src;
	}
}

/// If a monitors object exists, clearAllPolls
function pauseMonitoring () {
	if(typeof monitors != "undefined") {
		monitors.clearAllPolls(currentTab == 'Status');
		if($('pauseOption')) $('pauseOption').innerHTML='<a href="javascript:startMonitoring()">Monitoring PAUSED</a> |</span>';
	} else {
		if($('pauseOption')) $('pauseOption').innerHTML='';
	}
}

/// If a monitors object exists, startAll
function startMonitoring () {
	if(typeof monitors != "undefined") {
		monitors.startAll();
		if($('pauseOption')) $('pauseOption').innerHTML='<a href="javascript:pauseMonitoring()">Pause Monitoring</a> |</span>';
	} else {
		if($('pauseOption')) $('pauseOption').innerHTML='';
	}
}

/// Toggle display of frames in timecode
function toggleTC () {
	if(showFrames) {
		showFrames = false;
	} else {
		showFrames = true;
	}
}
