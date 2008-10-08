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

// --- Status Module Javascript ---

var monitors; // Global object for handling all monitors

// frame rate
if(typeof editrate == "undefined") var editrate = 25;

// Choose your logging level
var logRequests = false;
var logMemoryUsageChanges = false;
var logLatencyWarnings = false;
var logLoggingErrors = false;
// Set this to true to see what an alert looks like, without making something go drastically wrong!
var falseAlert = false;



// == Utility Functions == //

// Take an object, and sort the names of its properties, returning the sorted list as an array
function sortByValue(theObject,sortKey) {
	var keyArray = new Array();
	for (key in theObject) {
		keyArray.push(key);
	}
	return keyArray.sort(function(a,b) {return theObject[a][sortKey]-theObject[b][sortKey];});
}

// Convert a status message to a colour - allows easy management of global colour scheme
function Status_toCol (state) {
	switch(state)
	{
		case 'info':
			return '#e3e3ff';
		case 'ok':
			return '#e3ffe3';
		case 'warn':
			return '#eeeea3';
		case 'error':
			return '#eec3c3';
		case 'disabled':
			return '#eee';
		default:
			return '#eaeaea';
	}	
}

// Check if a value has changed, and if so, log it to insole
function logChange(field,callToMake,message) {
	// will call insole[callToMake], so default to insole.log while allowing caller to override to e.g. insole.alert
	if (isDefault(callToMake)) callToMake = 'log';
	// If message is specified, the message will be output, otherwise a message explaining the change will be generated
	if (isDefault(message)) message = false;
	var last = field.instance.getHistory(1);
	var current = field.instance.getHistory(0);
	var lastItem;
	var currentItem;
	var currentAll;
	var item = "";
	// An unspecified number of arguments is allowed, to specify what data item we are examining.
	// e.g. for current.monitorData.myFieldName.sillyPieceOfData you would call logChange(field,null,null,'myFieldName','sillyPieceOfData')
	// The code below drills down to the requested bit of data
	if(last && typeof last.monitorData != "undefined" && typeof last.monitorData[field.name] != "undefined") {
		lastItem = last.monitorData[field.name];
		for(var i=3;i < arguments.length;i++) {
			if(typeof lastItem[arguments[i]] != "undefined") {
				lastItem = lastItem[arguments[i]];
				if(i == arguments.length-1) {
					item += arguments[i];
				} else {
					item += arguments[i]+"-";
				}
			} else {
				if(logLoggingErrors) insole.log(field.monitor.friendlyName+" on "+field.instance.name+" : Error logging data for "+field.name+" - unable to current find previous data based on structure requested. OK with: "+item+", but died on: "+arguments[i]);
				return false;
			}
		}
	} else {
		if(logLoggingErrors && last) insole.log(field.monitor.friendlyName+" on "+field.instance.name+" : Error logging data for "+field.name+" - unable to find previous data");
		return false;
	}
	if(current && typeof current.monitorData[field.name] != "undefined") {
		currentItem = current.monitorData[field.name];
		currentAll = currentItem;
		for(var i=3;i < arguments.length;i++) {
			if(typeof currentItem[arguments[i]] != "undefined") {
				currentItem = currentItem[arguments[i]];
			} else {
				if(logLoggingErrors) insole.log(field.monitor.friendlyName+" on "+field.instance.name+" : Error logging data for "+field.name+" - unable to current find data based on structure requested. OK with: "+item+", but died on: "+arguments[i]);
				return false;
			}
		}
	} else {
		if(logLoggingErrors) insole.log(field.monitor.friendlyName+" on "+field.instance.name+" : Error logging data for "+field.name+" - unable to find current data");
		return false;
	}

	// If no message specified, make one
	if(!message) message = item+" changed from "+lastItem+" to "+currentItem;

	// // If we are looking at capture data, arguments[3] will need to be a channel number
	// If we are looking at recorder data, arguments[4] will need to be a channel number and arguments[3] a recorder name
	// If it is, it will have a tc property for the timecode
	// If so, display the timecode and channel in the message
	if (typeof arguments[3] != "undefined" && typeof currentAll[arguments[3]] != "undefined" && typeof currentAll[arguments[3]].tc != "undefined") {
		var tc = currentAll[arguments[3]].tc;
		message += " on channel "+arguments[3]+" at timecode: "+tc.h+":"+tc.m+":"+tc.s+":"+tc.f;
	}
	if (typeof arguments[3] != "undefined" && typeof currentAll[arguments[3]] != "undefined" && typeof arguments[4] != "undefined" && typeof current.monitorData['ring'][arguments[4]] != "undefined" && typeof current.monitorData['ring'][arguments[4]].tc != "undefined") {
		var tc = current.monitorData['ring'][arguments[4]].tc;
		message += " on channel "+arguments[4]+" of recorder "+arguments[3]+" at timecode: "+tc.h+":"+tc.m+":"+tc.s+":"+tc.f;
	}

	// Log the data
	if(lastItem != currentItem) {
		if (typeof insole[callToMake] == "function") {
			insole[callToMake](field.monitor.friendlyName+" on "+field.instance.name+" : "+message);
		} else if(logLoggingErrors) {
			insole.log(field.monitor.friendlyName+" on "+field.instance.name+" : Error logging data for "+field.name+" - invalid callToMake");
		}
	}
}

// Call an instance's AJAX stateChanged function
function callStateChanged(monitor,instance){
	if(typeof monitors != "undefined") monitors.m[monitor].i[instance].stateChanged();
}

// Call an instance's AJAX Timeout function
function callAjaxTimedout(monitor,instance){
	if(typeof monitors != "undefined") monitors.m[monitor].i[instance].ajaxTimedout();
}

// Call a monitor's availability AJAX stateChanged function
function callAvailabilityStateChanged(monitor,instance,display){
	monitor.availabilityStateChanged(instance,display);
}

// Call a monitor's availability AJAX Timeout function
function callAvailabilityTimedout(monitor,instance,display) {
	monitor.availabilityTimedout(instance,display);
}

// Display details when OSH icon is clicked
function showOSHDetails () {
	if(typeof monitors != "undefined") monitors.osh.displayDetails();
}



// == Data "Classes" == //
// (I put "classes" in quotes because JavaScript classes are not strictly like classes in proper OO languages like Java.
// In this case, the functions below are used purely as object templates, and the Prototypes of these objects don't get
// altered, but JavaScript is object-based rather than truly Object-Oriented.)

// A "class" for the whole lot of monitors and their data
function ingexMonitors (drawMonitors,startMonitors,containerID) {
	if (isDefault(drawMonitors)) drawMonitors = true;
	if (isDefault(startMonitors)) startMonitors = true;
	if (isDefault(containerID)) containerID = 'monitorsContainer';
	
	// DOM element to render all the monitors into
	this.container = containerID;
	
	// Paused State
	this.allPaused = false;
	
	// The monitors themselves
	this.m = new Object();
	// The order to display the monitors in
	this.order = new Array();
	// The nodes being monitored
	this.nodes = new Array();
	
	// Overall System Health
	this.osh = new ingexOSH();
	
	// Timecodes - references to them all, and a boolean as to whether to update them
	this.runTimecodes = false;
	this.allTimecodes = new Object();
	// A variable to hold the setInterval reference for the timecode updater
	this.tcIncrementer = false;
	
	// Names of all the monitors which have polls
	this.monitorPolls = new Object();
	
	// AJAX Timeout store for discover() function
	this.discoverTimeout = false;
	
	// Set all timecodes to increment
	this.startTimecodes = function() {
		if (!this.runTimecodes){
			this.runTimecodes = true;
			this.tcIncrementer = setInterval("monitors.incAllTimecodes()",(tcFreq*40));
		}
	}
	
	// Stop all timecodes from incrementing
	this.stopTimecodes = function() {
		if (this.runTimecodes){
			this.runTimecodes = false;
			clearInterval(this.tcIncrementer);
		}
	}
	
	// Stop all automatic updates from happening	
	this.clearAllPolls = function (updateDisplay) {
		if (isDefault(updateDisplay)) updateDisplay = false;
		this.allPaused = true;
		this.stopTimecodes();
		for(monitor in this.monitorPolls) {
			if (this.monitorPolls[monitor] == true) this.m[monitor].clearPoll(updateDisplay,false);
		}
	}
	
	// Increment all timecodes
	this.incAllTimecodes = function() {
		if (!this.allPaused){
			for (var timecode in this.allTimecodes) {
				if (this.allTimecodes[timecode]) this.allTimecodes[timecode].increment(tcFreq);
			}
		}
	}

	// Discover what monitors and instances there are to be queried and create appropriate objects
	this.discover = function () {
		var xmlHttp = getxmlHttp();
		// Add a fairly-unique number to the url, because if we don't, IE7 will cache pages.
		// This is bad in a system with constantly-updated data. The ideal solution would be to
		// use HTTP headers to disable caching (see http://www.mnot.net/cache_docs/ for example)
		// however this requires re-building Apache, which may not be possible or desirable.
		var now = new Date();
		var random = Math.floor(Math.random()*101);
		var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;
		
		function discoverTimedout() {
			xmlHttp.abort();
			this.discoverTimeout = false;
			insole.alert("AJAX timeout occurred when querying trying to discover monitors.");
			alert("Critical error: Could not load monitoring information - connection timed out contacting discoverMonitors.pl. Is your web server executing Perl correctly? Is the database connection available? Try re-loading the page, and if this error recurrs, contact your system administrator.")
		};
		
		xmlHttp.open("GET","/cgi-bin/ingex-modules/Status.ingexmodule/discoverMonitors.pl?requestID="+requestID,false);
		xmlHttp.send(null);
		if (this.discoverTimeout) {
			clearTimeout(this.discoverTimeout);
		}
		this.discoverTimeout = setTimeout(function(){discoverTimedout()},5000);
		if (xmlHttp.readyState==4)
		{
			clearTimeout(this.discoverTimeout);
			this.discoverTimeout = false;
			try {
				var tmp = JSON.parse(xmlHttp.responseText);
			} catch (e) {
				insole.alert("Could not load information about the monitors. Parse error in JSON data");
				insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
				alert("Critical error: Could not load monitoring information - invalid data received from discoverMonitors.pl. Is your web server executing Perl correctly? Is the database connection available? Try re-loading the page, and if this error recurrs, contact your system administrator.")
			}
			// Set up the monitor details object
			for(monitor in tmp.monitors) {
				this.m[monitor] = new ingexMonitor(monitor,tmp.monitors[monitor],this);
				// And populate OSH registers
				this.osh.errorRegister[monitor] = new Object();
				this.osh.warningRegister[monitor] = new Object();
				this.osh.instanceAvailability[monitor] = new Object();
			}
			// Get the sort order
			this.order = sortByValue(this.m,'sortOrder');
			// Save the node list
			this.nodes = tmp.nodes;
			
			// Populate the instances for each available monitor
			for (monitor in this.m) {
				for (var t in this.m[monitor].appliesTo) {
					var type = this.m[monitor].appliesTo[t]; 
					for (var node in tmp.nodes) {
						if (tmp.nodes[node].nodeType == type) {
							if(!this.m[monitor].i) this.m[monitor].i = new Object();
							this.m[monitor].i[node] = new ingexInstance(node,tmp.nodes[node].ip,tmp.nodes[node].volumes,this.m[monitor]);
							// Populate OSH registers
							this.osh.instanceAvailability[monitor][node] = false; // set false until we know it's available
							this.osh.unavailabilityCount++;
							this.osh.errorRegister[monitor][node] = new Object();
							this.osh.warningRegister[monitor][node]= new Object();
							// Populate the fields for this instance
							for (var field in this.m[monitor].fields){
								this.m[monitor].i[node].f[field] = new ingexField(field,this.m[monitor].i[node]);
								// Assign the field an appropriate draw() function based on its type
								this.m[monitor].i[node].f[field].processFieldData = typeHandlers[this.m[monitor].fields[field]];
								// Populate OSH registers
								this.osh.errorRegister[monitor][node][field] = false;
								this.osh.warningRegister[monitor][node][field]= false;
							}
						}
					}
				}
			}
		}
	}

	// Iterate through all monitors and start them querying without telling them to draw to screen
	this.startAll = function (unPause) {
		if (isDefault(unPause)) unPause = true;
		if(unPause) this.allPaused = false;
		if(falseAlert) insole.alert('Starting the monitors.');
		// Start each monitor
		for(var i in this.order) {
			if (typeof this.order[i] != "string") continue; // ignore the function we added to array's prototype
			if(unPause) this.m[this.order[i]].paused = false;
			this.m[this.order[i]].update();
		}
	}

	// Iterate through all monitors and start them querying, telling them to draw to screen
	this.drawAll = function () {
		// Clear the display
		this.container.$().innerHTML="";
		// Draw each monitor
		for(var i in this.order) {
			if (typeof this.order[i] != "string") continue; // ignore the function we added to array's prototype
			var monitor = this.m[this.order[i]];
			if (monitor.sortOrder > 0) { // if sortOrder is 0, don't include in drawing "all"
				// draw the monitor to the screen
				this.container.$().innerHTML += '<div class="monitorBlock" id="monitor_'+monitor.name+'"></div>';
				monitor.container = "monitor_"+monitor.name;
				monitor.visible = true;
			} else {
				// If not drawing a monitor, make sure it doesn't try to display
				monitor.visible = false;
			}
		}
		this.container.$().innerHTML += '<div class="displayOptions"><h5>Display Options:</h5> <a href="javascript:toggleTC()">Toggle timecode frame display</a></div>';
		this.startAll(false);
	}

	// Iterate through all monitors and tell them not to draw to screen
	this.stopDisplay = function () {
		for(var i in this.m) {
			this.m[i].visible = false;
		}
	}

	// On creation, discover the monitors
	this.discover();
	//Pre-load some images
	this.image = new Object();
	this.image.ok = new Image();
	this.image.ok.src = "/ingex/img/ok.gif";
	this.image.error = new Image();
	this.image.error.src = "/ingex/img/error.gif";
	this.image.info = new Image();
	this.image.info.src = "/ingex/img/info.gif";
	this.image.errorsmall = new Image();
	this.image.errorsmall.src = "/ingex/img/error_small.gif";
	this.image.recording = new Image();
	this.image.recording.src = "/ingex/img/recording.gif";
	// And if specified, draw the monitors too
	if (drawMonitors) {
		this.drawAll();
	} else if (startMonitors) {
		this.startAll();
	}
}

// A "class" for the Overall System Health
function ingexOSH (mainImageID,numInputsID,numInputsTextID) {
	// DOM IDs
	if (isDefault(mainImageID)) mainImageID = 'oshIcon';
	if (isDefault(numInputsID)) numInputsID = 'numInputs';
	if (isDefault(numInputsTextID)) numInputsTextID = 'numInputsText';
	this.mainImage = mainImageID;
	this.numInputs = numInputsID;
	this.numInputsText = numInputsTextID;
	
	// Images
	this.image = new Object();
	this.image.unknown = new Image();
	this.image.unknown.src = "/ingex/img/overall_unknown.gif";
	this.image.error = new Image();
	this.image.error.src = "/ingex/img/overall_error.gif";
	this.image.warn = new Image();
	this.image.warn.src = "/ingex/img/overall_warning.gif";
	this.image.ok = new Image();
	this.image.ok.src = "/ingex/img/overall_ok.gif";
	this.image.error_ua = new Image();
	this.image.error_ua.src = "/ingex/img/overall_error_ua.gif";
	this.image.warn_ua = new Image();
	this.image.warn_ua.src = "/ingex/img/overall_warning_ua.gif";
	this.image.ok_ua = new Image();
	this.image.ok_ua.src = "/ingex/img/overall_ok_ua.gif";
	
	// State information
	this.errorRegister = new Object();
	this.warningRegister = new Object();
	this.instanceAvailability = new Object();
	this.unavailabilityCount = 0;
	this.errorCount = 0;
	this.warningCount = 0;
	this.showing = false;
	this.channels = new Object();
	this.recording = new Object();
	this.showingRec = false;
	this.totalMonitors = 0;
	
	// Register an error
	this.error = function(theField){
		var monitor = theField.monitor.name;
		var instance = theField.instance.name;
		var field = theField.name;
		if (this.errorRegister[monitor][instance][field] != true) {
			// Register the error
			this.errorRegister[monitor][instance][field] = true;
			// Increase the error count
			this.errorCount++;
		}
		// Clear any warning already set for this field
		if(this.warningRegister[monitor][instance][field]) {
			this.warningRegister[monitor][instance][field] = false;
			this.warningCount--;
			if(this.warningCount < 0) this.warningCount = 0; // just in case we get out of sync - should never happen
		}
		this.draw();
	}
	
	// Register a warning
	this.warn = function(theField){
		var monitor = theField.monitor.name;
		var instance = theField.instance.name;
		var field = theField.name;
		if(this.warningRegister[monitor][instance][field] != true){
			// Register the warning
			this.warningRegister[monitor][instance][field] = true;
			// Increase the warning count
			this.warningCount++;
		}
		// Clear any error already set for this field
		if(this.errorRegister[monitor][instance][field]) {
			this.errorRegister[monitor][instance][field] = false;
			this.errorCount--;
			if(this.errorCount < 0) this.errorCount = 0; // just in case we get out of sync - should never happen
		}
		this.draw();
	}
	
	// Clear errors & warnings
	this.ok = function(theField){
		var monitor = theField.monitor.name;
		var instance = theField.instance.name;
		var field = theField.name;
		// Clear any warning or error for this field
		if(this.errorRegister[monitor][instance][field]) {
			this.errorRegister[monitor][instance][field] = false;
			this.errorCount--;
			if(this.errorCount < 0) this.errorCount = 0; // just in case we get out of sync - should never happen
		}
		if(this.warningRegister[monitor][instance][field]) {
			this.warningRegister[monitor][instance][field] = false;
			this.warningCount--;
			if(this.warningCount < 0) this.warningCount = 0; // just in case we get out of sync - should never happen
		}
		this.draw();
	}
	
	// Log an instance as available
	this.available = function(monitor,instance){
		if(!this.instanceAvailability[monitor][instance] && !monitors.m[monitor].paused){
			this.instanceAvailability[monitor][instance] = true;
			this.unavailabilityCount--;
			if(this.unavailabilityCount < 0) this.unavailabilityCount = 0;
			this.draw();
		}
	}
	
	// Log an instance as unavailable
	this.unavailable = function(monitor,instance){
		if(this.instanceAvailability[monitor][instance]) {
			this.instanceAvailability[monitor][instance] = false;
			this.unavailabilityCount++;
			this.draw();
		}
	}

	// Update the display of OSH
	this.draw = function() {
		var toShow = "";
		var title = "";
		if(this.unavailabilityCount != this.totalMonitors) {
			if(this.errorCount > 0) {
				toShow = "error";
				title = "Error - click for details.";
			} else if(this.warningCount > 0) {
				toShow = "warn";
				title = "Warning - click for details.";
			} else {
				toShow = "ok";
				title = "System OK - click for details.";
			}
			if (this.unavailabilityCount > 0) {
				toShow += "_ua";
				title += " NOTE: Some data may not be available. Click to see more";
			}
		} else {
			toShow = "unknown";
			title = "Status unknown - monitoring paused or unavailable";
			if(this.numInputsText.$()) this.numInputsText.$().innerHTML = "...";
		}
		if (toShow != this.showing) {
			this.mainImage.$().src = this.image[toShow].src;
			this.mainImage.$().title = title;
			this.mainImage.$().alt = title;
			this.showing = toShow;
		}
	}

	// Store and display the number of channels active on a node
	this.updateChannels = function(instance,numChannels){
		if(this.channels[instance] != numChannels) {
			this.channels[instance] = numChannels;
			this.drawChan();
		}
	}

	// Store and display the number of channels being recorded on a node	
	this.updateRecording = function(instance,numChannels){
		if(this.recording[instance] != numChannels) {
			this.recording[instance] = numChannels;
			this.drawChan();
		}
	}
	
	// Display video input and recording info
	this.drawChan = function() {
		var totalRec = 0;
		for(var instance in this.recording) {
			if (typeof this.recording[instance] == "number") totalRec += this.recording[instance];
		}
		
		var totalChan = 0;
		for(var instance in this.channels) {
			if (typeof this.channels[instance] == "number") totalChan += this.channels[instance];
		}
		
		var text = "";
		if(totalRec > 0) {
			text = totalRec+" <span class='small'>of</span> "+totalChan;
			if(!this.showingRec && this.numInputs.$()) this.numInputs.$().className = 'numInputs rec';
			this.showingRec = true;
		} else {
			text = totalChan;
			if(this.showingRec && this.numInputs.$()) this.numInputs.$().className = 'numInputs';
			this.showingRec = false;
		}
		if(this.numInputsText.$()) this.numInputsText.$().innerHTML = text;
	}

	// Show the details of what errors there are
	this.displayDetails = function() {
		var html = "";
		if (this.errorCount > 0 || this.warningCount > 0) {
			html += "<h1>Overall System Health</h1>";
			html += "<p>Error count: "+this.errorCount+". Warning count: "+this.warningCount+".</p>";
			html += "<p>The following errors and warnings are reported:</p><ul>";
			for (var monitor in this.errorRegister) {
				for (var instance in this.errorRegister[monitor]) {
					for (var field in this.errorRegister[monitor][instance]) {
						if(this.errorRegister[monitor][instance][field]) {
							html += "<li class='error'>Error with "+monitors.m[monitor].friendlyName+" on "+instance+". (Data field: "+field+")</li>";
						}
					}
				}
			}
			for (var monitor in this.warningRegister) {
				for (var instance in this.warningRegister[monitor]) {
					for (var field in this.warningRegister[monitor][instance]) {
						if(this.warningRegister[monitor][instance][field]) {
							html += "<li>Warning from "+monitors.m[monitor].friendlyName+" on "+instance+". (Data field: "+field+")</li>";
						}
					}
				}
			}
			html += "</ul>";
		}
		if (this.unavailabilityCount > 0) {
			html += "<h1>Data Availability</h1>";
			html += "<p>Data currently unavailable from:</p><ul>";
			for (var monitor in this.instanceAvailability) {
				for (var instance in this.instanceAvailability[monitor]) {
					if (this.instanceAvailability[monitor][instance] == false) {
						html += "<li>"+monitors.m[monitor].friendlyName+" on "+instance+"</li>";
					}
				}
			}
			html += "</li>";
		}
		if (html != "") {
			html = "<p>To see full status details, <a href='javascript:getTab(\"Status\",false,true,true);'>click here</a> or on the Status tab at the top right corner of the page.</p>" + html;
			getTab('Status',false,true,html);
		} else {
			getTab('Status',false,true,true);
		}
	}

}

// A "class" for an individual monitor and its instances/data
function ingexMonitor (myName,monitorInfo,myParentObject) {
	// Basic Monitor Info
	this.friendlyName = monitorInfo.friendlyName;
	this.sortOrder = monitorInfo.sortOrder;
	this.fields = monitorInfo.fields;
	this.appliesTo = monitorInfo.appliesTo;
	this.pollTime = monitorInfo.pollTime;
	this.queryType = monitorInfo.queryType;
	this.queryLoc = monitorInfo.queryLoc;
	if (!isDefault(monitorInfo.thresholds)) {
		this.thresholds = monitorInfo.thresholds;
	} else {
		this.thresholds = new Object();
	}
	
	//Name and Parent
	this.name = myName;
	this.myParent = myParentObject;
	
	this.myParent.osh.totalMonitors++;
	
	// DOM elements for displaying this monitor
	this.container = false;
	this.stopButton = false;
	
	// Instances & leeched
	this.i = false;
	this.leeches = false;
	
	// State Info
	this.paused = false;
	this.visible = false;
	this.showingError = false;
	this.showingLoadingMessage = false;
	this.latencyWarning = 0;
	this.leechWarningGiven = false;
	
	// Poll
	this.poll = false;
	
	// For use with timecodes... wait for new data before incrementing
	this.waitForNewData = true;
	
	// Verify that we can contact the monitor
	this.checkAvailability = function (instance,display) {
		var instanceExists = false;
		for (inst in this.i) {
			if (inst == instance) {
				instanceExists = true;
			}
		}
		if(!instanceExists) {
			$(display).innerHTML = "<span class='error'>Cannot resolve instance</span>.";
			return false;
		}
		
		var monitor;
		if(this.queryType == "leech") {
			monitor = this.myParent.m[this.queryLoc];
		} else {
			monitor = this;
		}

		var urlToGet = "/cgi-bin/ingex-modules/Status.ingexmodule/queryMonitor.pl?";
		urlToGet += "action=availability&monitor="+monitor.name+"&instances="+instance+":"+monitor.i[instance].ip;
		if (monitor.queryType == "http") {
			urlToGet += "&queryType=http&queryLoc="+monitor.queryLoc;
		} else {
			urlToGet += "&queryType=standard";
		}

		// Add a fairly-unique number to the url, because if we don't, IE7 will cache pages.
		// This is bad in a system with constantly-updated data. The ideal solution would be to
		// use HTTP headers to disable caching (see http://www.mnot.net/cache_docs/ for example)
		// however this requires re-building Apache, which may not be possible or desirable.
		var now = new Date();
		var random = Math.floor(Math.random()*101);
		var requestID = now.getHours()+"-"+now.getMinutes()+"-"+now.getSeconds()+"-"+now.getMilliseconds()+"-"+random;
		urlToGet += "&requestID="+requestID;

		var monToCall = this;

		if(!this.i[instance].availXmlHttp) this.i[instance].availXmlHttp = getxmlHttp();
		this.i[instance].availXmlHttp.onreadystatechange = function (){callAvailabilityStateChanged(monToCall,instance,display);};

		this.i[instance].availXmlHttp.open("GET",urlToGet,true);
		this.i[instance].availXmlHttp.send(null);
		this.i[instance].availabilitytimeout = setTimeout(function(){callAvailabilityTimedout(monToCall,instance,display)},5000);
	}
	
	this.availabilityTimedout = function(instance,display) {
		this.i[instance].availXmlHttp.abort();
		$(display).innerHTML = "<span class='error'>Unavailable</span><br /><span class='small indent'>Request timed out.</span>";
		this.i[instance].availabilitytimeout = false;
		insole.error("AJAX timeout occurred when testing availability of "+this.name+" on "+instance);
		this.myParent.osh.unavailable(this.name,instance);
	}
	
	this.availabilityStateChanged = function(instance,display) {
		if (this.i[instance].availXmlHttp.readyState==4)
		{
			var monitor;
			if(this.queryType == "leech") {
				monitor = this.myParent.m[this.queryLoc];
			} else {
				monitor = this;
			}
			if (this.i[instance].availabilitytimeout) {
				clearTimeout(this.i[instance].availabilitytimeout);
				this.i[instance].availabilitytimeout = false;
			}
			if (this.i[instance].availXmlHttp.status == 200) {
				try {
					var tmp = JSON.parse(this.i[instance].availXmlHttp.responseText);
				} catch(e) {
					clearTimeout(this.i[instance].availabilitytimeout);
					this.i[instance].availabilitytimeout = false;
					insole.alert("Error getting availability from "+this.friendlyName+" on "+instance+". Invalid data received. (JSON Parse error.)");
					insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
					$(display).innerHTML = "<span class='error'>Unavailable</span><br /><span class='small indent'>Node gave invalid response.</span>";
					monitors.osh.unavailable(monitor.name,instance);
				}
				if (tmp[monitor.name][instance].availability == true) {
					$(display).innerHTML = "Available.";
					monitors.osh.available(this.name,instance);
				} else if(typeof tmp[monitor.name][instance].requestError != "undefined") {
					$(display).innerHTML = "<span class='error'>Unavailable</span><br /><span class='small indent'>Could not contact monitor.</span>";
					insole.error("Parsable JSON but with requestError specified when checking availability of "+this.friendlyName+" on "+instance);
					monitors.osh.unavailable(this.name,instance);
				} else {
					$(display).innerHTML = "<span class='error'>Unavailable</span><br /><span class='small indent'>Node gave invalid response.</span>";
					insole.error("Parsable JSON but unexpected content when checking availability of "+this.friendlyName+" on "+instance);
					insole.log("JSON was: "+this.i[instance].availXmlHttp.responseText);
					monitors.osh.unavailable(this.name,instance);
				}
			} else {
				$(display).innerHTML = "<span class='error'>Unavailable</span><br /><span class='small indent'>Error contacting web server.</span>";
				insole.error("HTTP status: "+this.i[instance].availXmlHttp.status+" when checking availability of "+this.friendlyName+" on "+instance);
			}
		}
	}
	
	// Display the monitor data. (If the monitor has not been drawn, it will be drawn first).
	this.draw = function (json,status,instanceWithError) {
		if (isDefault(instanceWithError)) instanceWithError = false;
		var monitorName;
		if(this.queryType == "leech") {
			monitorName = this.queryLoc;
		} else {
			monitorName = this.name;
		}
		var monitorData;
		var running = (!this.paused && !this.myParent.allPaused);
		if (running) {
			if (status == 200) {
				try {
					monitorData = JSON.parse(json);
				} catch (e) {
					insole.alert("Error getting data from "+this.friendlyName+". Invalid data received. (JSON Parse error).");
					insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
					if(this.visible) {
						this.container.$().innerHTML = '<h1>'+this.friendlyName+'</h1><p class="error">Error: Invalid data received from monitor. See <a href="javascript:insole.popout()">log</a> for details</p>';
						this.showingError = true;
					}
					for(instance in this.i) {
						this.myParent.osh.unavailable(monitorName,instance);
					}
				}
			} else {
				if (this.visible && (this.showingLoadingMessage || this.showingError || this.container.$().innerHTML == "")) {
					// If my container is empty, draw some basic contents
					var func = "monitors.m['" + this.name + "'].createPoll()";
					this.container.$().innerHTML = '<div class="monitorStopRefresh" id="stop_'+this.name+'"><a href="javascript:'+func+'">[stopped]</a></div><h1>'+this.friendlyName+'</h1>';
					this.stopButton = 'stop_'+this.name;
					this.stopButton.$().style.visibility = 'visible';
					this.showingLoadingMessage = false;
					this.showingError = false;
				}
				var theError = new Object();
				theError.requestError = status;
				if(typeof this.i[instanceWithError] != "undefined") this.i[instanceWithError].draw(theError);
				return false;
			}
		}
		// If we have valid data, process it
		if (this.visible && (this.showingLoadingMessage || this.showingError || (this.container && this.container.$().innerHTML == ""))) {
			// If my container is empty, or if I was showing an error but now have valid data, draw some basic contents
			this.container.$().innerHTML = '<div class="monitorStopRefresh" id="stop_'+this.name+'"></div><h1>'+this.friendlyName+'</h1>';
			this.showingLoadingMessage = false;
			this.showingError = false;
			this.stopButton = 'stop_'+this.name;
			if(this.poll) {
				this.stopButton.$().style.visibility = 'visible';
				if(running) {
					var func = "monitors.m['" + this.name + "'].clearPoll()";
					this.stopButton.$().innerHTML='<a href="javascript:'+func+'">x</a>';
				} else {
					var func = "monitors.m['" + this.name + "'].createPoll()";
					this.stopButton.$().innerHTML='<a href="javascript:'+func+'">[stopped]</a>';
				}
			} else if (this.queryType=="leech") {
				this.stopButton.$().style.visibility = 'visible';
				var func = 'javascript:alert("This monitor is updated at the same time as the '+this.queryLoc+' monitor. Pause or restart that monitor, and the same action will be applied to this one.");';
				this.stopButton.$().innerHTML="<span class='small'>Updates controlled by "+this.queryLoc+". (<a style='display: inline;' href='"+func+"'>Help</a>)</span>";
			}
		}
		for (var instance in this.i) {
			if(running) {
				if (typeof monitorData[monitorName] != "undefined" && typeof monitorData[monitorName][instance] != "undefined") {
					this.i[instance].draw(monitorData[monitorName][instance]);
					this.i[instance].addHistory(monitorData[monitorName][instance]);
				}
			} else {
				this.i[instance].draw(this.i[instance].getHistory());
			}
		}
		if(running) this.createPoll(); // createPoll checks to see if a poll is needed, and creates one if so
	}

	// Get data from the monitor and display it.
	this.update = function () {
		if(this.i) {
			if (this.visible && this.container.$().innerHTML == "") {
				this.container.$().innerHTML = "<h1>"+this.friendlyName+"</h1><span id='"+this.container+"_loading'></span>";
				showLoading(this.container+"_loading");
				this.showingLoadingMessage = true;
				this.showingError = false;
			}
			if (!this.paused && !this.myParent.allPaused) {
				if (this.queryType != "leech") {
					for(instance in this.i) {
						this.i[instance].query();
					}
				} else {
					if(typeof this.myParent.m[this.queryLoc] != "undefined") {
						this.myParent.m[this.queryLoc].addLeech(this.name);
					} else if (!this.leechWarningGiven) {
						insole.alert("Invalid leech host defined for "+this.name);
						this.leechWarningGiven = true;
					}
				}
			} else {
				// call draw() to show that monitor is paused
				this.draw(false,false);
			}
		} else {
			if (this.visible && this.container.$().innerHTML == "") {
				this.container.$().innerHTML = "<h1>"+this.friendlyName+"</h1><p>No applicable nodes configured. Have you configured all the PCs in use? Please check the <a href='javascript:getTab(\"config\",true,true,true)'>Configure page</a>.</p>";
				this.showingLoadingMessage = false;
				this.showingError = true;
			}
		}
	}

	// Create a poll to auto-update
	this.createPoll = function () {
		if (this.pollTime && this.myParent.monitorPolls[this.name] != true) {
			if(this.queryType != "leech") {
				this.paused = false;
				for(var leech in this.leeches) {
					if(typeof this.myParent.m[leech] != "undefined") {
						this.myParent.m[leech].paused = false;
					}
				}
				this.update();
				// register the poll with all monitors object
				this.myParent.monitorPolls[this.name] = true;
				// set the stop button to clear the poll
				if(this.visible) {
					var func = "monitors.m['" + this.name + "'].clearPoll()";
					this.stopButton.$().innerHTML='<a href="javascript:'+func+'">x</a>';
					this.stopButton.$().style.visibility = 'visible';
				}
				// set up the poll
				func = "monitors.m['" + this.name + "'].update()";
				this.poll = setInterval(func,this.pollTime);
			} else {
				if(this.visible) {
					var func = 'javascript:alert("This monitor is updated at the same time as the '+this.queryLoc+' monitor. Pause or restart that monitor, and the same action will be applied to this one.");';
					this.stopButton.$().innerHTML="<span class='small'>Updates controlled by "+this.queryLoc+". (<a style='display: inline;' href='"+func+"'>Help</a>)</span>";
					this.stopButton.$().style.visibility = 'visible';
				}
			}
		}
	}

	// Stop auto-update
	this.clearPoll = function (updateDisplay, logToInsole) {
		if (isDefault(updateDisplay)) updateDisplay = true;
		if (isDefault(logToInsole)) logToInsole = true;
		this.myParent.monitorPolls[this.name] = false;
		// clear the poll
		this.paused = true;
		if (this.poll) clearInterval(this.poll);
		if(this.visible && updateDisplay) {
			// set the stop button to restart the poll
			var func = "monitors.m['" + this.name + "'].createPoll()";
			this.stopButton.$().innerHTML='<a href="javascript:'+func+'">[stopped]</a>';
			// remove colour from background to show data is not current
			for (instance in this.i){
				if(this.i[instance].container && this.i[instance].container.$()) this.i[instance].container.$().style.backgroundColor = Status_toCol('disabled');
			}
		}
		// on restart, timecodes should wait for a new time from the server, not increment the out of date one
		this.waitForNewData = true;
		if(logToInsole) insole.log("Paused "+this.friendlyName);
		for(var instance in this.i) {
			this.myParent.osh.unavailable(this.name,instance);
		}
		for(var leech in this.leeches) {
			if(typeof this.myParent.m[leech] != "undefined") {
				this.myParent.m[leech].paused = true;
				for(var instance in this.myParent.m[leech].i) {
					this.myParent.osh.unavailable(leech,instance);
				}
			}
		}
	}
	
	// Gradual back-off if updates are taking too long to happen
	this.slowDown = function(level){
		if (level == 3 && this.latencyWarning == 2) {
			insole.alert("Stopped displaying data for "+this.name+" because of continued latency problems. Any data show would likely be dangerously out of date.");
			this.clearPoll();
			this.latencyWarning = 0;
			if(this.visible) {
				var func = "monitors.m['" + this.name + "'].createPoll()";
				this.container.$().innerHTML = '<div class="monitorStopRefresh" id="stop_'+this.name+'"><a href="javascript:'+func+'">[stopped]</a></div><h1>'+this.friendlyName+'</h1><p class="error">Error: The time taken to receive data has consistently been long. Polling stopped to avoid showing invalid data. You can try again by clicking the [stopped] button above.</p>';
				this.stopButton.$().style.visibility = 'visible';
				this.showingError = true;
			}
		} else if (level == 2 && this.latencyWarning == 1) {
			this.pollTime = this.pollTime * 1.5;
			insole.log("CRITICAL WARNING: Poll time increased for a second time on monitor "+this.name+" due to repeated latency problems. New poll time:"+this.pollTime,"alert");
			this.latencyWarning = 2;
		} else if(level == 1 && this.latencyWarning == 0) {
			this.pollTime = this.pollTime * 1.5;
			insole.warn("Poll time increased on monitor "+this.name+" due to repeated latency problems. New poll time:"+this.pollTime,"alert");
			this.latencyWarning = 1;
		}
	}
	
	// Add another monitor to the list of leeches
	this.addLeech = function(leechName) {
		if (!this.leeches) this.leeches = new Object();
		if (typeof this.leeches[leechName] == "undefined") this.leeches[leechName] = true;
	}
	
}

// A "class" for a specific instance and its data
function ingexInstance (myName,myIP,myVolumes,myParentMonitor) {
	this.name = myName; // Instance name
	this.ip = myIP; // IP address for http-queried monitors
	// the volumes whose free space should be monitored (really only relevant to system monitor) :
	this.volumes = false;
	if (!isDefault(myVolumes)) this.volumes = myVolumes.split(",");
	this.monitor = myParentMonitor; // the monitor which this is an instance of
	this.container = false; // the DOM element in which to render this instance
	this.showingError = false;
	this.f = new Object(); // associative array of all fields for this instance
	this.xmlHttp = getxmlHttp();
	this.availXmlHttp = getxmlHttp();
	this.ajaxtimeout = false; // timeout for ajax requests to update instance data
	this.availabilitytimeout = false; // timeout for ajax requests to check availability
	this.timeoutWarning = 0; // gets incremented if a request has not returned the time a new one is started
	
	// Data history allows us to watch trends if needed
	this.history = new Array();
	
	// Query the monitor, and the call draw() with the results
	this.query = function (action) {
		if (isDefault(action)) action = "basicInfo";
		if (!this.monitor.paused && !this.monitor.myParent.allPaused) {
			if(this.ajaxtimeout == false && (this.xmlHttp.readyState == 0 || this.xmlHttp.readyState == 4)) {
				var urlToGet = "/cgi-bin/ingex-modules/Status.ingexmodule/queryMonitor.pl?";
				urlToGet += "action="+action+"&monitor="+this.monitor.name+"&instances="+this.name+":"+this.ip;
				if (this.monitor.queryType == "http") {
					urlToGet += "&queryType=http&queryLoc="+this.monitor.queryLoc;
				} else {
					urlToGet += "&queryType=standard";
				}
			
				// Add a fairly-unique number to the url, because if we don't, IE7 will cache pages.
				// This is bad in a system with constantly-updated data. The ideal solution would be to
				// use HTTP headers to disable caching (see http://www.mnot.net/cache_docs/ for example)
				// however this requires re-building Apache, which may not be possible or desirable.
				var now = new Date();
				var random = Math.floor(Math.random()*101);
				var requestID = now.getHours()+"-"+now.getMinutes()+"-"+now.getSeconds()+"-"+now.getMilliseconds()+"-"+random;
				urlToGet += "&requestID="+requestID;
				// Send the request
				this.xmlHttp.open("GET",urlToGet,true);
				// n.b. must define the onreadystatechange function after opening the request, otherwise it won't get called!
				this.xmlHttp.onreadystatechange = function(){callStateChanged(myParentMonitor.name,myName)};
				this.xmlHttp.send(null);
				if (logRequests){
					insole.log("Update request sent to "+this.monitor.name+" on "+this.name);
				}
			
				// Set a timeout for the request...
				this.ajaxtimeout = setTimeout("callAjaxTimedout('"+this.monitor.name+"','"+this.name+"')",5000);
				
				// No latency problem, so decrease warning level
				if(this.timeoutWarning > 0) this.timeoutWarning--;
				
			} else if (!this.showingError) {
				// If there's already a timeout present, or the xmlHttp object is not ready,
				// that means the last request hasn't returned yet.
				// This may indicate latency problems, so handle this...
				// (n.b. it's if(!this.showingError) so that we don't trigger back-off on the whole monitor if a machine is unavailable
				this.timeoutWarning++;
				if(logLatencyWarnings) insole.warn("The query latency for the last update request to "+this.monitor.name+" on "+this.name+" was longer than the poll interval. Current warning level: "+this.timeoutWarning);
				if(this.timeoutWarning > 7) {
					this.monitor.slowDown(3);
					this.timeoutWarning = 0;
				} else if(this.timeoutWarning > 4) {
					this.monitor.slowDown(2);
				} else if (this.timeoutWarning > 2) {
					this.monitor.slowDown(1);
				}
			}
		}
	}
	
	this.stateChanged = function () {
		if (this.xmlHttp.readyState==4)
		{
			if (this.ajaxtimeout != false) {
				clearTimeout(this.ajaxtimeout);
				this.ajaxtimeout = false;
			}
			if (logRequests){
				insole.log("Response received from "+this.monitor.name+" on "+this.name);
			}
			var resp = this.xmlHttp.responseText;
			var stat = this.xmlHttp.status;
			this.monitor.draw(resp,stat);
			if(this.monitor.leeches != false) {
				for (var leech in this.monitor.leeches) {
					if (this.monitor.leeches[leech]) this.monitor.myParent.m[leech].draw(resp,stat);
				}
			}
		}
	}
	
	this.ajaxTimedout = function() {
		var stat = this.xmlHttp.readyState;
		if (stat == 4) stat = "4 (completed).";
		this.xmlHttp.abort();
		this.ajaxtimeout = false;
		insole.error("AJAX timeout occurred when querying "+this.monitor.name+" on "+this.name+". readyState: "+stat);
		this.monitor.draw(null,'Request timed out when contacing monitor. reasyState: '+stat,this.name);
		for(var leech in this.monitor.leeches){
			this.monitor.myParent.m[leech].draw(null,'Request timed out when contacing monitor. readyState: '+stat,this.name);
		}
	};
	
	// Process & display the instance data. (If the instance has not been drawn, it will be drawn first).
	this.draw = function (instanceData) {
		if (this.monitor.visible && (!this.container || !this.container.$())) {
			// If my container hasn't already been drawn, or no longer exists, draw it
			this.monitor.container.$().innerHTML += '<div class="instanceBlock" id="instance_'+this.monitor.name+'_'+this.name+'"></div>';
			this.container = "instance_"+this.monitor.name+"_"+this.name;
			this.container.$().innerHTML = '<h2>'+this.name+'</h2>';
			this.showingError = false;
		}
		if (typeof instanceData.requestError != "undefined") {
			this.monitor.myParent.osh.unavailable(this.monitor.name,this.name);
			if(this.monitor.visible) {
				// If there's a request error, display a message
				this.container.$().innerHTML = "<h2>"+this.name+"</h2><span class='error'>" + instanceData.requestError + "</span>";
				this.showingError = true;
				this.container.$().style.backgroundColor = Status_toCol('error');
			}
		} else {
			this.monitor.myParent.osh.available(this.monitor.name,this.name);
			if (this.monitor.visible && this.showingError) {
				// If we've shown an error previously, but now have some valid data, redraw the instance
				this.container.$().innerHTML = '<h2>'+this.name+'</h2>';
				this.showingError = false;
				// Don't worry about re-setting the background colour, as this happens below based on the state of the data
			}
			//iterate each field
			for (var field in this.f) {
				var dataError = this.f[field].draw(instanceData);
				// If an error/warning was issued, update the instance state
				if (dataError == "error") {
					instanceData.state = "error";
				} else if (dataError == "warn" && instanceData.state != "error") {
					instanceData.state = "warn";
				}
			}
			if(this.monitor.visible && !this.monitor.paused && !this.monitor.myParent.allPaused) {
				this.container.$().style.backgroundColor=Status_toCol(instanceData.state);
			} else if (this.monitor.visible) {
				this.container.$().style.backgroundColor=Status_toCol('disabled');
			}
		}
	}
	
	// Add data to the history
	this.addHistory = function(newData){
		this.history.push(newData);
		if(this.history.length > 10) {
			this.history = this.history.slice(1);
		}
	}
	
	// Get data from the history - the latest by default, or an arbitrary element
	this.getHistory = function(offset){
		if (isDefault(offset)) offset = 1; // The most recent entry is the current data, so get the one before
		var offsetToGet = offset + 1; // array keys start from zero, therefore subtract 1 from history.length
		if(typeof this.history[this.history.length-offsetToGet] != "undefined"){
			return(this.history[this.history.length-offsetToGet]);
		} else {
			return false;
		}
	}
}

// A "class" for a specific field
function ingexField (myName,myParentInstance) {
	this.name = myName; // field name
	this.instance = myParentInstance; // instance of which this field is a part
	this.monitor = this.instance.monitor; // monitor of which this field's instance is an instance
	this.container = false; // DOM element into which this field is rendered
	this.elements = new Object(); // allows storage of multiple DOM elements for different bits of data
	this.timecodes = new Object(); // current state of any timecode(s) used in this field
	
	// Handle setting up a DOM element to draw into and overall field errors.
	this.draw = function(instanceData){
		if (this.monitor.visible && (!this.container || !this.container.$())) {
			// If my container hasn't already been drawn, or no longer exists, draw it
			this.instance.container.$().innerHTML += '<div class="fieldBlock" id="field_'+this.monitor.name+'_'+this.instance.name+'_'+this.name+'"></div>';
			this.container = 'field_'+this.monitor.name+'_'+this.instance.name+'_'+this.name;
			this.container.$().innerHTML = '';
		}
		var error = this.processFieldData(instanceData,this.monitor.visible);
		if (this.monitor.visible && error == -1) this.container.$().innerHTML = '<span class="error">Data could not be displayed</span>';
		return error;
	}
	
	// Does the meat of drawing the data
	this.processFieldData = function(instanceData,visible){
		// Blank function. Should always be replaced at creation time with a specific type handler.
		insole.error("Field "+this.name+" appears to have no typeHandler defined.");
		// Return -1 to indicate unhandled data
		return -1;
	}
}

// A "class" for a timecode
function ingexTimecode (myField,myChannel,myRecorder) {
	if (isDefault(myRecorder)) myRecorder = false;
	this.field = myField;
	this.recorderName = myRecorder;
	this.channelName = myChannel;
	if (this.recorderName != false){
		this.descriptor = this.field.monitor.name+"-"+this.field.instance.name+"-"+this.field.name+"-"+myRecorder+"-"+myChannel;
	} else {
		this.descriptor = this.field.monitor.name+"-"+this.field.instance.name+"-"+this.field.name+"-"+myChannel;
	}

	this.h = 0;
	this.m = 0;
	this.s = 0;
	this.f = 0;
	this.stopped = false;

	this.set = function(h,m,s,f,stopped) {
		this.h = h;
		this.m = m;
		this.s = s;
		this.f = f;
		this.stopped = stopped;
	}
	
	this.increment = function(numToInc) {
		if (isDefault(numToInc)) numToInc = 1;
		var draw = false;
		if (this.field.monitor.visible && !this.stopped && !this.field.monitor.paused && !this.field.monitor.waitForNewData) {
			// Do the actual incrementing
			this.f += numToInc;
			if (this.f > editrate) {
				this.f -= editrate;
				this.s++;
				draw = true;
				if (this.s > 59) {
					this.s -= 59;
					this.m++;
					if (this.m > 59) {
						this.m -= 59;
						this.h++;
					}
				}
			}
			// Format as 2 digit numbers
			var h; var m; var s; var f;
			if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
			if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
			if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
			if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
			
			// Draw the timecode
			if (this.field.monitor.visible) {
				// if container not valid, attempt to find it
				if(!this.container || !this.container.$()) {
					if (this.recorderName != false) {
						this.container = this.field.elements[this.recorderName][this.channelName].timecode;
					} else {
						this.container = this.field.elements[this.channelName].timecode;
					}
				}
				// draw
				var fr = "--";
				if (showFrames) fr = f;
				if ((showFrames || draw) && this.container && this.container.$()) this.container.$().innerHTML = h+":"+m+":"+s+":"+fr;
			}
			
		} else if (this.field.monitor.visible && this.field.monitor.paused && this.container && this.container.$()) {
			this.container.$().innerHTML = "<span class='message'>timecode unavailable<br />(monitoring paused)</span>";
		} else if (this.field.monitor.visible && this.field.monitor.waitForNewData && this.container && this.container.$()) {
			showLoading(this.container);
		} else if (this.field.monitor.visible && this.stopped && this.container && this.container.$()) {
			// Draw an error
			this.container.$().innerHTML = "<span class='error'>Timecode stopped</span>";
		} else if (!this.field.monitor.visible) {
			this.field.monitor.waitForNewData = true;
		}
	}
	
	this.remove = function() {
		this.field.monitor.myParent.allTimecodes[this.descriptor] = false;
	}
	
	// Register with the all monitors object so I can be incremented
	this.field.monitor.myParent.allTimecodes[this.descriptor] = this;
}



// == Loader Functions == //

// Specify our onLoad function
var onLoad_Status = function () {
	if (typeof monitors == "undefined") {
		// Create our monitors object
		monitors = new ingexMonitors();
	} else {
		monitors.stopDisplay();
		monitors.container.$().innerHTML="";
		monitors.drawAll();
	}
}
loaderFunctions.Status = onLoad_Status;

// alternative onLoad to show a single monitor rather than the overview
function Status_loadMonitor (monitor) {	
	if (typeof monitors == "undefined") {
		// Create our monitors object, but display no monitors
		monitors = new ingexMonitors(false);
	} else {
		monitors.stopDisplay();
	}
	
	// Clear the display
	monitors.container.$().innerHTML='<div class="monitorBlock" id="monitor_'+monitor+'"></div><div class="displayOptions"><h5>Display Options:</h5> <a href="javascript:toggleTC()">Toggle timecode frame display</a></div>';
	monitors.m[monitor].container = "monitor_"+monitor;
	// Draw the monitor
	monitors.allPaused = false;
	monitors.m[monitor].visible = true;
	monitors.m[monitor].update();
}



// == Type Handlers == //

// Set up ana array to register typeHandlers into. These are functions which get added to field objects at creation
// time based on field type, and which do the meat of displaying field data in the interface.
var typeHandlers = new Array();

// The simplest type handler
function Status_process_simpleText (instanceData,visible) {
	if(!visible) return false;
	this.container.$().innerHTML = instanceData.monitorData[this.name];
}
typeHandlers.simpleText = Status_process_simpleText;