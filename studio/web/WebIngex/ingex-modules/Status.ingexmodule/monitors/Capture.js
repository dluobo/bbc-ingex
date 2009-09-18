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

// --- Capture Monitor Javascript (part of Status module) ---

// Display video info - size and formats
function Status_process_videoInfo (instanceData, visible) {
	// Log changes to insole
	logChange(this,null,null,'videosize');
	logChange(this,null,null,'pri_video_format');
	logChange(this,null,null,'sec_video_format');
	
	// If not visible, no need to draw anything, so exit
	if(!visible) return false;
	
	var data = instanceData.monitorData[this.name];
	
	// If not drawm, draw the layout
	if(this.container.$().innerHTML == ''){
		var h;
		h  = "<img src='/ingex/img/info.gif' style='float:left;'/>";
		h += "<span style='font-weight:bold;'>Video Size : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_size'></span><br />";
		h += "<span style='font-weight:bold;'>Primary Format : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_pri'></span><br />";
		h += "<span style='font-weight:bold;'>Secondary Format : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_sec'></span>";
		this.container.$().innerHTML = h;
		this.elements.secondary = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_sec";
		this.elements.primary = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_pri";
		this.elements.videosize = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_size";
	}
	
	// Fill in the data
	this.elements.videosize.$().innerHTML = data.videosize;
	this.elements.primary.$().innerHTML = data.pri_video_format;
	this.elements.secondary.$().innerHTML = data.sec_video_format;
}
typeHandlers.videoInfo = Status_process_videoInfo;

// Display capture health - process running plus heartbeat
function Status_process_captureHealth (instanceData, visible) {
	var data = instanceData.monitorData[this.name];
	var ok = true;
	var c_msg = "Running";
	var h_msg = "Present";
	
	// check the status..
	if(data.capture_dead == 1){
		ok = false;
		c_msg = "<span class='error'>DEAD</span>";
		logChange(this,'alert','Capture died','capture_dead');
	} else {
		logChange(this,null,'Capture recovered','capture_dead');
	}
	if(data.heartbeat_stopped == 1){
		ok = false;
		h_msg = "<span class='error'>STOPPED</span>";
		logChange(this,'alert','Heartbeat stopped','heartbeat_stopped');
	} else {
		logChange(this,null,'Heartbeat started','heartbeat_stopped');
	}
	
	if(!ok){
		monitors.osh.error(this);
	} else {
		monitors.osh.ok(this);
	}
	
	// If not visible, don't draw anything, just exit
	if (!visible) return false;
	
	// If not drawm, draw the layout
	if(this.container.$().innerHTML == ''){
		var h = "<img id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_img' src='/ingex/img/ok.gif' style='float:left;'/>";
		h += "<span style='font-weight:bold;'>Capture process : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_capture'></span><br />";
		h += "<span style='font-weight:bold;'>Heartbeat : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_heartbeat'></span><p style='clear:both;'></p>";
		this.container.$().innerHTML = h;
		this.elements.heartbeat = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_heartbeat";
		this.elements.capture = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_capture";
		this.elements.img = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_img";
	}

	// Fill in the data
	if(!ok){
		this.elements.img.$().src = this.monitor.myParent.image.error.src;
	} else {
		this.elements.img.$().src = this.monitor.myParent.image.ok.src;
	}
	this.elements.capture.$().innerHTML = c_msg;
	this.elements.heartbeat.$().innerHTML = h_msg;
}
typeHandlers.captureStatus = Status_process_captureHealth;

function Status_process_channelState (instanceData, visible) {
	var errorCode = "ok";
	if(typeof this.timecodes == "undefined" || this.timecodes == false) this.timecodes = new Array();
	for (channel in instanceData.monitorData[this.name]) {
		if (typeof this.elements[channel] == "undefined" || this.elements[channel] == false) this.elements[channel] = new Object();
		if (typeof this.timecodes[channel] == "undefined" || this.timecodes[channel] == false) this.timecodes[channel] = new ingexTimecode(this,channel);
	}
	
	if(visible && this.container.$().innerHTML == ''){
		var h = "<table class='simpleTable'>";
		for (channel in instanceData.monitorData[this.name]) {
			var chan = instanceData.monitorData[this.name][channel];
			h += "<tr><td>";
			h += "<img id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_img' src='/ingex/img/error.gif'/>";
			h += "</td><td>";
			h += "<span style='font-weight:bold;'>Channel "+channel+" ("+chan.source_name+")</span>";
			h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_temp'></span><br />";
			h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_state'></span>";
			h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_timecode'></span>";
			h += "<div class ='audioOuterBar'>";
			h += "<div class='audioInnerBarL' id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_audioL'></div>";
			h += "<div class='audioInnerBarR' id='field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_audioR'></div>";
			h += "</div></td></tr>";
			this.elements[channel].audioL = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_audioL";
			this.elements[channel].audioR = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_audioR";
			this.elements[channel].image = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_img";
			this.elements[channel].state = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_state";
			this.elements[channel].temp = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_temp";
			this.elements[channel].timecode = "field_"+this.monitor.name+"_"+this.instance.name+"_"+channel+"_timecode";
		}
		h += "</table>";
		this.container.$().innerHTML = h;
	}
	
	for (channel in instanceData.monitorData[this.name]) {
		var channelError = Status_processCaptureChannel(this,channel,instanceData.monitorData[this.name][channel],null,null,visible);
		if (channelError == "error") {
			// any channel error causes an overall error
			errorCode = "error";
		} else if (errorCode = "ok" && channelError != "ok") {
			// channel warning causes overal warning unless there's already an overall error (caused by a different channel)
			errorCode = channelError;
		}
	}
	this.monitor.waitForNewData = false;
	
	if(visible) {
		// Call startTimecodes on the all monitors object. If the timecodes are not already being incremented, they now will be.
		this.monitor.myParent.startTimecodes();
	}
	
	if(errorCode == "error") {
		monitors.osh.error(this);
	} else {
		monitors.osh.ok(this);
	}
	
	var numChannels = 0;
	for (var channel in instanceData.monitorData[this.name]) {
		if (instanceData.monitorData[this.name][channel].signal_ok == 1) {
			numChannels++;
		}
	}
	monitors.osh.updateChannels(this.instance.name,numChannels);
	
	return errorCode;
}
typeHandlers.channelStateArray = Status_process_channelState;

// Display channel data - timecode, audio and status
function Status_processCaptureChannel (field,channel,channelData,recorder,recordError,visible) {
	var chan = channelData;
	if (isDefault(recordError)) recordError = false;
	
	var last = field.instance.getHistory();
	
	// Because (if we're displaying a recorder's data)
	// we need to access some data from a different field (i.e. the ring), we have problems calling
	// functions like logChange. Therefore create a proxyField which, if we're drawing capture, will be the
	// same as field. But if we're drawing a recorder, we'll concoct a fake field containing the ring's name,
	// instance and monitor.
	var proxyField;
	
	// Depending on whether this is being called by the capture monitor or the recorders monitor, the location
	// of the DOM element IDs and timecodes will be different
	var elements;
	var timecode;
	if (isDefault(recorder)) {
		elements = field.elements[channel];
		timecode = field.timecodes[channel];
		recorder = false;
		proxyField = field;
	} else {
		elements = field.elements[recorder][channel];
		timecode = field.timecodes[recorder+"-"+channel];
		proxyField = new Object();
		proxyField.name = 'ring';
		proxyField.instance = field.instance;
		proxyField.monitor = field.monitor;
	}
	
	// Set the timecode
	var channelError = "ok";
	var timecodeStatus = "ok";
	if (chan.tc.stopped == 1) {
		timecodeStatus = "error";
		timecode.stopped = true;
		logChange(proxyField,'alert','Timecode stopped',channel,'tc','stopped');
	} else {
		logChange(proxyField,null,'Timecode started',channel,'tc','stopped');
		timecode.set(chan.tc.h,chan.tc.m,chan.tc.s,chan.tc.f,false);
	}
	// No need to actually draw the timecode, as this will happen many times a second anyway when the global timecode
	// incrementer kicks in.
	var methodToCall = 'log';
	var myMessage = 'Video appeared';
	if(chan.signal_ok != 1) {
		methodToCall = 'warn';
		myMessage = 'Video disappeared';
	}
	logChange(proxyField,methodToCall,myMessage,channel,'signal_ok');
	
	if(timecodeStatus == "error" && chan.signal_ok) {
		//If there's video but no timecode, we have a problem!
		channelError = "error";
	} else if (timecodeStatus == "error" && !chan.signal_ok && channelError != "error") {
		//If there's no timecode but also no video, it may be that the channel just isn't in use, 
		// therefore just warn. (As long as there isn't already a bigger error)
		channelError = "warn";
	} else if (timecodeStatus == "warn" && channelError != "error"){
		// If there's a timecode warning and not already found a bigger error, set a warning.
		channelError = "warn";
	}
	
	if(typeof elements.temp != "undefined" && elements.temp.$()) {
		var threshold = field.monitor.thresholds.temperature;
		if(chan.temperature < threshold) {
			if(visible) elements.temp.$().innerHTML = " <span class='small'>- Card temperature: "+chan.temperature+"&deg;</span>";
			if(typeof field.instance.getHistory(1) != "undefined" ){
				// if there is previous history data...
				if (typeof field.instance.getHistory(1).monitorData != "undefined" && typeof field.instance.getHistory(1).monitorData[field.name] != "undefined" && field.instance.getHistory(1).monitorData[field.name].thresholdBroken == true) {
					// if the threshold was broken last time, inform about it being ok now
					insole.log(field.monitor.friendlyName+" on "+field.instance.name+": Temperature now safe on capture card for channel"+channel);
				}
			}
		} else {
			if(visible) elements.temp.$().innerHTML = "- <span class='error'>Card temperature: "+chan.temperature+"&deg;</span>";
			channelError = "error";
			var log = false;
			if(typeof field.instance.getHistory(0) != "undefined"){
				// if there's a history entry for the current data (there should be!) log the broken threshold
				field.instance.getHistory(0).monitorData[field.name].thresholdBroken = true;
			}
			if(typeof field.instance.getHistory(1) != "undefined" ){
				// if there is previous history data...
				if (typeof field.instance.getHistory(1).monitorData[field.name].thresholdBroken == "undefined") {
					// if the threshold wasn't broken last time, warn about it being broken now
					log = true;
				}
			} else {
				// if there is no previous history data, we have probably just loaded the page, so warn about the broken threshold
				log = true;
			}
			insole.alert(field.monitor.friendlyName+" on "+field.instance.name+": Temperature of capture card at dangerous level for channel"+channel);
			
		}
	}
	
	// If not visible, don't draw anything!
	if(!visible) return channelError;
	
	// Display the info - set the image plus the audio info and any errors
	if (chan.signal_ok && channelError == "ok" && !recordError) {
		elements.image.$().src = field.monitor.myParent.image.ok.src;
		elements.state.$().innerHTML = "";
	} else {
		elements.image.$().src = field.monitor.myParent.image.error.src;
		if(chan.signal_ok){
			elements.state.$().innerHTML = "";
		} else {
			elements.state.$().innerHTML = "<span class='error'>No video</span><br />";
		}
	}
	
	elements.audioL.$().style.width = chan.audio_power[0] + 98 + 'px'; // -97dB = no audio, show 1px
	elements.audioR.$().style.width = chan.audio_power[1] + 98 + 'px'; // -97dB = no audio, show 1px
	return channelError;
}

var onLoad_Status_Capture = function () {
	Status_loadMonitor('Capture');
}
loaderFunctions.Status_Capture = onLoad_Status_Capture;	