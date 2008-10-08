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
 */

/// Simple convenience function to add some syntactic sugar to retrieving DOM elements by ID
/// @param id The ID of the element to get
/// @return the DOM element, or false if none is available
function $(id) 
{
	var element = document.getElementById(id);
	if(element){
		return element;
	} else {
		return false;
	}
}

/// Add a function to the String prototype which gets the DOM element whose ID is
/// the String's contents. This allows us to store strings which describe an element's
/// ID and then easily access that element. This avoids storing references to DOM 
/// elements in variables directly, as I have found this to be flaky, to say the least
/// example: var myElement = "myDivID"; myElement.$().innerHTML = "This is my Div";
/// @return the DOM element, or false if none is available
String.prototype.$ = function() 
{
	var element = document.getElementById(this);
	if(element){
		return element;
	} else {
		return false;
	}
}

/// Simple  function to test if a variable is a "default"
/// i.e. when looking at parameters to a function, if a value has not been set or is equal to null
/// it is assumed that the default value should be used instead
/// most often used in the context: if (isDefault(variable)) variable = value;
/// which will set variable=value if and only if variable is not already set to something meaningful
/// @param v The variable to test
/// @return boolean describing whether the value is a "default" or not
function isDefault(v)
{
	if(typeof v == "undefined" || v == null) {
		return true;
	}
	return false; 
}

/// Convert a page name to a full URL, e.g. index -> /myWebsiteName/index.html
/// Uses the prefix and suffix defined in Injax preferences
/// @param pageName the page name to be converted
/// @return the full URL
function pageToURL (pageName) {
	if (injax.customURLCreator != false) {
		return injax.customURLCreator(pageName);
	} else {
		return injax.urlPrefix + pageName + injax.urlSuffix;
	}
}

/// The "Insole" Integrated Console object
function Insole() {
	this.allMessages = new Array();
	this.sysLog = false;
	this.statusTimeout = false;
	this.writeToWindow = false;
	
	// State
	this.specialVisible = false;
	this.expanded = false;
	this.alertTimeout = false;
	
	this.log = function(message, myClass) {
		if (isDefault(myClass)) myClass = false;
		window.status = message;
		if(this.statusTimeout) clearTimeout(this.statusTimeout);
		this.statusTimeout = setTimeout("window.status=''",8000);
		var now = new Date();
		// Format as 2 digit numbers
		var h = now.getHours();
		var m = now.getMinutes();
		var s = now.getSeconds();
		if (h < 10) { h = "0" + h; }
		if (m < 10) { m = "0" + m; }
		if (s < 10) { s = "0" + s; }
		message = h + ":" + m + ":" + s + " - " + message;
		if(this.sysLog && injax.insole.logToSysLog) this.sysLog.log(message);
		
		this.allMessages.push(message);
		if(this.allMessages.length > 2000) this.allMessages.shift(); // delete first element if we're over 2000 to avoid spiralling memory use
		
		if(this.writeToWindow) {
			if(!myClass) {
				window.open('','insolewindow').document.getElementById("insoleWindowOutput").innerHTML += "<p>" + message + "</p>";
			} else {
				window.open('','insolewindow').document.getElementById("insoleWindowOutput").innerHTML += "<p class='"+myClass+"'>" + message + "</p>";
			}
		}
		if(injax.insole.outputDiv.$()) {
			if(injax.insole.newItemsAtTop){
				if(!myClass) {
					injax.insole.outputDiv.$().innerHTML = "<p>" + message + "</p>" + injax.insole.outputDiv.$().innerHTML;
				} else {
					injax.insole.outputDiv.$().innerHTML = "<p class='"+myClass+"'>" + message + "</p>" + injax.insole.outputDiv.$().innerHTML;
				}
			} else {
				if(!myClass) {
					injax.insole.outputDiv.$().innerHTML += "<p>" + message + "</p>";
				} else {
					injax.insole.outputDiv.$().innerHTML += "<p class='"+myClass+"'>" + message + "</p>";
				}
			}
		}
	}
	
	this.warn = function(message) {
		this.log("WARNING: " + message);
	}
	
	this.error = function(message) {
		this.log("ERROR: " + message);
	}
	
	this.alert = function(message) {
		message = "ALERT: " + message;
		this.log(message,"alert");
		if(injax.insole.container.$()) {
			if(this.expanded) {
				injax.insole.container.$().className =  "consoleExpanded alert";
			} else {
				injax.insole.container.$().className =  "consoleMinimised alert";
			}
		}
		if(this.alertTimeout) clearTimeout(this.alertTimeout);
		this.alertTimeout = setTimeout("insole.clearAlert()",injax.insole.alertTime);
		if(!this.expanded) {
				if(injax.insole.showHide.$()) injax.insole.showHide.$().innerHTML = '<a style="display:block;" href="javascript:insole.show()">Show Insole (Unread Alert!)</a>';
				if(injax.insole.specialDiv.$()) {
					this.specialVisible = true;
					injax.insole.specialDiv.$().innerHTML = message;
					injax.insole.specialDiv.$().style.display = "block";
				}
		}
	}
	
	this.clearAlert = function() {
		if(this.expanded) {
			injax.insole.container.$().className =  "consoleExpanded";
		} else {
			injax.insole.container.$().className =  "consoleMinimised";
		}
		this.alertTimeout = false;
		if(this.specialVisible && injax.insole.specialDiv.$()) {
			this.specialVisible = false;
			injax.insole.specialDiv.$().innerHTML = "";
			injax.insole.specialDiv.$().style.display = "none";
		}
	}
	
	this.show = function() {
		if(injax.insole.container.$()){
			if(this.specialVisible && injax.insole.specialDiv.$()) {
					this.specialVisible = false;
					injax.insole.specialDiv.$().innerHTML = "";
					injax.insole.specialDiv.$().style.display = "none";
			}
			if(window.isIE6 == true) { // if IE6, show popup
				// Re-set button text to remove any messages about waiting alerts
				injax.insole.showHide.$().innerHTML = '<a href="javascript:insole.popout()">Show Insole</a>';
				// remove red colouring
				injax.insole.container.$().className =  "consoleMinimised";
				this.popout();
			} else {
				injax.insole.container.$().className =  "consoleExpanded";
				injax.insole.showHide.$().innerHTML = '<a href="javascript:insole.popout()">Pop Out</a> | <a href="javascript:insole.clear()">Clear History</a> | <a href="javascript:insole.hide()">Hide Insole</a>';
				if (injax.insole.outputDiv.$()) {
					injax.insole.outputDiv.$().style.display = 'block';
				}
				this.expanded = true;
				injax.contentPane.$().className =  injax.contentClass+" clearance";
			}
		}
	}
	
	this.hide = function() {
		if(injax.insole.container.$()){
			injax.insole.container.$().className =  "consoleMinimised";
			injax.insole.showHide.$().innerHTML = '<a style="display:block;" href="javascript:insole.show()">Show Insole</a>';
			if (injax.insole.outputDiv.$()) {
				injax.insole.outputDiv.$().style.display = 'none';
			}
			this.expanded = false;
			injax.contentPane.$().className =  injax.contentClass;
		}
	}
	
	this.popout = function() {
		newwindow=window.open('','insolewindow','height=400,width=600,scrollbars=1,resizable=1');
		var tmp = newwindow.document;
		tmp.write('<html><head><title>Insole</title>');
		tmp.write('<link rel="stylesheet" href="injax.css">');
		tmp.write('</head><body onUnload="opener.insole.writeToWindow = false;" class="consolePopout"><div id="insoleWindowOutput">');
		for(var message in this.allMessages) {
			if(typeof this.allMessages[message] == "string") tmp.write("<p>" + this.allMessages[message] + "</p>");
		}
		tmp.write('</div></body></html>');
		tmp.close();
		this.writeToWindow = true;
	}
	
	this.clear = function() {
		if(injax.insole.outputDiv.$()) {
			injax.insole.outputDiv.$().innerHTML = "";
		}
	}
	
	// Create a console object with a blank log() function in order to avoid console.log() calls creating problems
	// in browsers which don't support it.
	if(typeof console == "undefined") {
		console = new Object();
	}
	if(!console.log) {
		console.log = function() {return false;}
	} else {
		this.sysLog = console;
	}
}

/// Simple function to get an HTTP Request object for any browser
/// @return the request object
function getxmlHttp()
{
	try
	{
		// Get the XMLHttpRequest object for sensible browsers - Firefox, Safari, Opera etc
		xmlHttp=new XMLHttpRequest();
	}
	catch (e)
	{
		// Internet Explorer is a pain, so we have to do things differently...
		try
		{
			xmlHttp=new ActiveXObject("Msxml2.XMLHTTP");
		}
		catch (e)
		{
			try
			{
				xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");
			}
			catch (e)
			{
				// If all else fails, apologise to the user and tell them to get a half-decent browser
				alert("Your browser does not support AJAX. Please upgrade your browser to Safari (3 or later), Opera (8 or later), Firefox (2 or later) or Internet Explorer (6 or later).");
				return false;
			}
		}
	}
	return xmlHttp;
}

/// Write an error message into the innerHTML of a DOM element.
/// @param elementName the ID of the DOM element to write to
/// @param url the URL associated with the error (e.g. the url you were requesting which failed to return) - can be null for no URL
/// @param error the main error message
/// @param message any descriptive text
function drawError (elementName,url,error,message) 
{
	var html="<h1 class='error'>Error</h1><p>An error has occurred while processing your request. The error was:</p><p class='error'>";
	html += error+"</p>";
	if(!isDefault(url)){
		html += "<p>URL: "+url+"</p>";
	}
	html += "<p>"+message+"</p>";
	$(elementName).innerHTML = html;
}

/// A function to update the contents of a DOM element with the
/// response from an HTML request. Give it the name of the element to update
/// (not a reference to the element itself), the URL to request, a vars string
/// for GET data (e.g. "project=Ingex&dept=Research%20and%20Innovation").
/// You may also specify a loaderFunction.
/// This function will try to display and hide a loadingBox element, to indicate to the user
/// that a request is taking place. You will probably want to implement such a box by specifying a DOM element in Injax config.
/// @param elementName the name of the element whose contents will be replaced with the result of the request
/// @param urlToGet the url to make a request to
/// @param vars a URL-encoded string of variables (GET data)
/// @param postString a POST-encoded string of variables (setting this non-null automatically selects a POST-type request)
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed
/// @param historyString Add a hash to the URL in order to add it to the browser history and allow bookmarking (the string to log, or false)
/// @param allowContinue If false, the call is synchronus (blocking), if true (or unspecified) the request is asynchronus
function getElementWithVars(elementName,urlToGet,vars,postString,loaderFunction,historyString,allowContinue)
{
	if (isDefault(vars)) vars = null;
	if (isDefault(postString)) postString = null;
	if (isDefault(loaderFunction)) loaderFunction = null;
	if (isDefault(historyString)) historyString = false;
	if (isDefault(allowContinue)) allowContinue = true;

	injax.requestsWaiting++;
	if(injax.requestsWaiting>0){
		if(injax.setCursor) document.body.style.cursor='progress';
		if(injax.loadingBox && injax.loadingBox.$()) injax.loadingBox.$().style.display='inline';
	}
	
	// Get an XMLHttpRequest object
	var xmlHttp = getxmlHttp();
	
	// Construct the final URL
	if(vars){
		urlToGetWithVars = urlToGet+"?"+vars;
	} else {
		urlToGetWithVars = urlToGet;
	}
	
	// Add a fairly-unique number to the url, because if we don't, IE7 will cache pages.
	// This is bad in a system with constantly-updated data. The ideal solution would be to
	// use HTTP headers to disable caching (see http://www.mnot.net/cache_docs/ for example)
	// however this requires re-building Apache, which may not be possible or desirable.
	var now = new Date();
	var random = Math.floor(Math.random()*101);
	var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
	if(!vars){
		urlToGetWithVars += "?";
	} else {
		urlToGetWithVars += "&";
	}
	urlToGetWithVars += "requestID="+requestID;

	var responseReceived = function()
	{
		if(xmlHttp.readyState==4)
		{
			if (xmlHttp.status == 200) {
				$(elementName).innerHTML=xmlHttp.responseText;
				if(loaderFunction != null){
					if(injax.customCallLoader){
						injax.customCallLoader(loaderFunction);
					} else {
						if(typeof loaderFunctions[loaderFunction] != "undefined") {
							loaderFunctions[loaderFunction]();
						}
					}
				}
			} else if (xmlHttp.status == 404) {
				drawError(elementName,urlToGet,"404 - Page Not found.","Unfortunately, the information requested cannot be found. Please try again and if you get the same error, inform your system administrator.");
			} else if (xmlHttp.status == 500) {
				drawError(elementName,urlToGet,"500 - Internal Server Error.","Unfortunately, the server has encountered an error. Please try again and if you get the same error, inform your system administrator.");
			} else {
				drawError(elementName,urlToGet,"Unknown Error.","Unfortunately, there has been an unknown error. Please try again and if you get the same error, inform your system administrator.");
			}
			injax.requestsWaiting--;
			if(injax.requestsWaiting<=0){
				if(injax.setCursor) document.body.style.cursor='default';
				if(injax.loadingBox && injax.loadingBox.$()) injax.loadingBox.$().style.display='none';
			}
  		}
	}
	
	// Add to history
	addToHistory(historyString);

	// Issue the request appropriately based on whether it's synchronus or asynchronus
	// (Most browsers will allow you to do it the same way, just changing the appropriate
	// parameter to the open() function, however Firefox won't call the
	// onreadystatechange function if doing a synchronus request, so for Firefox's sake
	// we must place the code after the send() call if doing a synchronus request.
	if(allowContinue) {
		// Define what to do on request completion (update the content pane)
		xmlHttp.onreadystatechange=responseReceived;
	
		// Issue the request
		if(postString != null) {
			xmlHttp.open("post",urlToGetWithVars,allowContinue);
			xmlHttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
			xmlHttp.send(postString);
		} else {
			xmlHttp.open("GET",urlToGetWithVars,true);
			xmlHttp.send(null);
		}
	} else {
		// Issue the request
		if(postString != null) {
			xmlHttp.open("post",urlToGetWithVars,allowContinue);
			xmlHttp.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
			xmlHttp.send(postString);
		} else {
			xmlHttp.open("GET",urlToGetWithVars,false);
			xmlHttp.send(null);
		}
		responseReceived();
	}
}

/// A convenience function for getting an element but with no GET data or loaderFunction.
/// @see getElementWithVars
/// @param elementName the element whose contents will be replaced with the result of the request
/// @param urlToGet the URL to make the request to
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed 
/// @param historyString Add a hash to the URL in order to add it to the browser history and allow bookmarking (the string to log, or false)
/// @param allowContinue If false, the call is synchronus (blocking), if true (or unspecified) the request is asynchronus
function getElement(elementName,urlToGet,loaderFunction,historyString,allowContinue)
{
	getElementWithVars(elementName,urlToGet,null,null,loaderFunction,historyString,allowContinue);
}

/// A convenience function to automate the processing of updating the ContentPane
/// with the results of an HTTP Request. Unlike getPageWithVars, getPage has no facility for adding GET data.
/// @see getPageWithVars
/// @param pageName the filename of the page to get, excluding the extension
/// @param fullPathSupplied set true if you supply a full path to the file rather than just a page name
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed 
/// @param logToHistory Add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param allowContinue If false, the call is synchronus (blocking), if true (or unspecified) the request is asynchronus
function getPage(pageName, fullPathSupplied, loaderFunction, logToHistory, allowContinue)
{
	getPageWithVars(pageName, null, fullPathSupplied, loaderFunction, logToHistory, allowContinue)
}

/// A convenience function to automate the processing of updating the ContentPane
/// with the results of an HTTP Request.
/// @see getElementWithVars
/// @param pageName the filename of the page to get, excluding the extension
/// @param vars a URL-encoded string of variables (GET data)
/// @param fullPathSupplied set this to true if you supply a full path to the file rather than just a page name
/// @param loaderFunction the index of the loaderFunctions array which will be executed once the request has completed 
/// @param logToHistory Add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param allowContinue If false, the call is synchronus (blocking), if true (or unspecified) the request is asynchronus
function getPageWithVars(pageName, vars, fullPathSupplied, loaderFunction, logToHistory, allowContinue)
{
	if (isDefault(fullPathSupplied)) fullPathSupplied = false;
	if (isDefault(logToHistory)) logToHistory = true;
	
	var url;
	if(fullPathSupplied){
		url = pageName;
	} else {
		url = pageToURL(pageName);
	}
	if(logToHistory == true) var historyString = createHistoryString(pageName, vars, fullPathSupplied);
	getElementWithVars(injax.contentPane,url,vars,null,loaderFunction,historyString,allowContinue);
}

/// Create a string for history logging from the page name, vars and full path info.
/// @param pageName the name of the page to load (no extension)
/// @param vars a GET data string of variables, or false
/// @param fullPathSupplied boolean - set to true if pageName is a full path rather than a simple page name
/// @return the history string
function createHistoryString (pageName, vars, fullPathSupplied) {
	if (injax.customHistoryString) {
		injax.customHistoryString(pageName, vars, fullPathSupplied);
	} else {
		if (isDefault(fullPathSupplied)) fullPathSupplied = false;
		if(fullPathSupplied){
			return pageName+"::"+fullPathSupplied;
		} else {
			return pageName;
		}
	}
}

/// Show a hidden DOM element (for block level elements only)
/// @param elementName the ID of the DOM element to show
/// @param oneAtATime defaults to true. If true, a record will be kept of which element is visible, and making a subsequent call will hide the visible element before showing a new one.
function show (elementName,oneAtATime) {
	if (isDefault(oneAtATime)) oneAtATime = true;
	if(oneAtATime){
		if(injax.visibleElement != ""){
			if($(injax.visibleElement)){
				$(injax.visibleElement).style.display='none';
			}
		}
		injax.visibleElement = elementName;
	}
	$(elementName).style.display='block';
}

/// Traverses the DOM tree from an element downwards, adding form elements' contents to a POST string. Used by sendForm.
/// @see sendForm
/// @param obj the object (DOM element) to traverse
/// @return a POST string describing the element's form elements
function addChildNodes(obj){
	var poststr="";
	var i=0;
	if(obj){
		for (i=0; i<obj.childNodes.length; i++) {
			if (obj.childNodes[i].tagName == "INPUT") {
				if (obj.childNodes[i].type == "text" || obj.childNodes[i].type == "hidden") {
					poststr += obj.childNodes[i].name + "=" + escape(obj.childNodes[i].value) + "&";
				} else	if (obj.childNodes[i].type == "checkbox") {
					if (obj.childNodes[i].checked) {
						poststr += obj.childNodes[i].name + "=" + escape(obj.childNodes[i].value) + "&";
					} else {
						poststr += obj.childNodes[i].name + "=&";
					}
				} else	if (obj.childNodes[i].type == "radio") {
					if (obj.childNodes[i].checked) {
						poststr += obj.childNodes[i].name + "=" + escape(obj.childNodes[i].value) + "&";
					}
				} else if(obj.childNodes[i].type == "submit") {
					if(obj.childNodes[i].name==whichPressed){
						poststr += obj.childNodes[i].name + "=" + obj.childNodes[i].value + "&";
					}
				}
			} else if (obj.childNodes[i].tagName == "SELECT") {
				var sel = obj.childNodes[i];
				if(sel.length != 0 && sel.options[sel.selectedIndex]){
					poststr += sel.name + "=" + escape(sel.options[sel.selectedIndex].value) + "&";
				}
			} else if (obj.childNodes[i].tagName == "TEXTAREA") {
				poststr += obj.childNodes[i].name + "=" + escape(obj.childNodes[i].value) + "&";
			} else if (obj.childNodes[i].tagName == "") {
				var sel = obj.childNodes[i];
				if(sel.options[sel.selectedIndex])
				poststr += sel.name + "=" + escape(sel.options[sel.selectedIndex].value) + "&";
			}
			poststr += addChildNodes(obj.childNodes[i]);
		}
	}
	return poststr;
}

/// Grab all the data from a form and send the POST request to get the response. Put
/// the response into the contentPane.
/// @param formName the ID of the form element
/// @param submitPage the filename of the page to submit the form to, excluding the extension
/// @param logToHistory if true, add a hash to the URL in order to add it to the browser history and allow bookmarking
/// @param outputID the ID of a DOM element into which the results of the form submission will be placed
function sendForm(formName, submitPage, logToHistory, outputID) {	
	if (isDefault(outputID)) outputID = injax.contentPane;
	if (isDefault(logToHistory)) logToHistory = true;
	
	if(logToHistory) addToHistory(submitPage,true);
		
	var poststr = addChildNodes($(formName));
	var urlToGet = pageToURL(submitPage);
	getElementWithVars(outputID,urlToGet,null,poststr,null,null,true);
}

/// Add an element to the history by adding the logEntry parameter to the URL as a hash. Uses RSH's dhtmlHistory.add() - (see RSH - http://code.google.com/p/reallysimplehistory/ )
/// @param historyString the text to add to the URL (excluding the hash sign itself)
/// @param isFormSubmission set true if submitting a form, and ::submit will be added to the end, allowing correct handling on refreshing the page
function addToHistory (historyString,isFormSubmission) {
	if (injax.customAddToHistory) {
		injax.customAddToHistory(historyString,isFormSubmission);
	} else {
		// If it's a form submission, add the info to the URL, 
		// so that if someone hits refresh, the form can be cancelled or processed appropriately
		if (isDefault(isFormSubmission)) isFormSubmission = false;
		if (isFormSubmission) historyString += "::submit";
		if (historyString) dhtmlHistory.add(historyString);
	}
}

/// Act on a change to the location (see RSH - http://code.google.com/p/reallysimplehistory/ )
/// @param newLocation The new location to load
/// @historyData JSON data to add to the history store
function urlChange (newLocation, historyData) {
	if(injax.customURLChange) {
		injax.customURLChange(newLocation, historyData);
	} else {
		//No specified location means the home page
		if(newLocation == ''){
			newLocation = 'index';
		}
	
		//Split the location string into:
		//	Page
		//	FullPathSpecified
		var chunks = newLocation.split("::");
		
		//Call getPage with the info from the URL
		getPage(chunks[0], chunks[1]);
	}
}

/// Act on a change to the location (see RSH - http://code.google.com/p/reallysimplehistory/ )
function pageLoad() {
	var query = window.location.hash;
	// Skip the leading # 
	if (query.substring(0, 1) == '#') {
		query = query.substring(1);
	}
	urlChange(query, null);
}

//========================== 
//  Set up some variables
//==========================
		// Create an instance of Insole
		window.insole = new Insole();

		// To allow a function to be called after an AJAX request is made - write a function, register it as a
		// loaderFunction, and then when making your call to getElementWithVars (or an equivelant function)
		// specify the loaderFunction to be called once the response has been received.
		window.loaderFunctions = new Object();

		// Variable for deciding which form button was pressed. Used by sendForm()
		window.whichPressed=0;

		// Create a DHTMLHistory object (see RSH - http://code.google.com/p/reallysimplehistory/ )
		window.dhtmlHistory.create(
		{
			toJSON: function(o) {
				return JSON.stringify(o);
			},
			fromJSON: function(s) {
				return JSON.parse(s);
			}
		});

//===========================
/// onLoad/onUnload functions
//===========================
/// onLoad function for the page - initialise the history, respond to any URL data and run any custom page loader defined in Injax config
window.onload = function() {	
	dhtmlHistory.initialize();
	dhtmlHistory.addListener(urlChange);
	
	// Log a welcome message
	if(typeof insole != "undefined") insole.log("Welcome to Insole.");
	
	// Run any custom defined loading function
	if(injax.customLoader) injax.customLoader();
	
	pageLoad();
};
/// onUnload function checks for presence of an Insole popout and closes it when you leave the page
window.onunload = function() {
	if(injax.customOnUnload) injax.customOnUnload();
	if(window.insole) {
		if(window.insole.writeToWindow) {
			window.open('','insolewindow').close();
		}
	}
}