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

// --- Recorders Monitor Javascript (part of Status module) ---

function Status_process_recorderState (instanceData,visible) {
	var data = instanceData.monitorData[this.name];
	
	var numChannels = 0;	
	for (var recorder in data) {
		if(typeof this.elements[recorder] == "undefined" || this.elements[recorder] == false) this.elements[recorder] = new Object();
		for (channel in data[recorder]) {
			if(typeof this.elements[recorder][channel] == "undefined" || this.elements[recorder][channel] == false) this.elements[recorder][channel] = new Object();
			if(typeof this.timecodes[recorder+"-"+channel] || this.timecodes[recorder+"-"+channel] == false) this.timecodes[recorder+"-"+channel] = new ingexTimecode(this,channel,recorder);
			if (data[recorder][channel].recording == 1) numChannels++;
		}
	}
	monitors.osh.updateRecording(this.instance.name,numChannels);
	
	// If not drawn (& the monitor is visible), draw the layout
	if(visible && this.container.$().innerHTML == '' || (this.showingNoRecorders == true && data.noRecorders != true)){
		this.showingNoRecorders = false;
		var html = "";
		for (var recorder in data) {
			var rec = data[recorder];
			html += "<div class='recorder_recorder'>";
			html += "<h3>Recorder: "+recorder+"</h3>";
			// Use tables to make data line up nicely. Should possibly be done with CSS in a future revision
			html += "<table>";
			for (channel in data[recorder]) {
				// For each channel
				var chan = instanceData.monitorData.ring[channel];
				var channelDesc = 'field_'+this.monitor.name+"_"+this.instance.name+"_"+recorder+"_"+channel;
				html += "<tr><td rowspan=3><img id='"+channelDesc+"_img' src='/ingex/img/error.gif'/></td>";
				if (channel == "quad") {
					html += "<th colspan=2>Quad Split</th>"
				} else {
					html += "<th colspan=2>Channel "+channel+" : "+chan.source_name+"</th>";
				}
				html += "</tr>";
				html += "<tr class='recorder_channel'>";
				html += "<td>Input:</td><td>Recordings:</td></tr>";
				if (channel != "quad") {
					html += "<tr><td style='text-align:center; padding-right:20px;'><span id='"+channelDesc+"_state'></span><span id='"+channelDesc+"_timecode'></span>";
					html += "<div class ='audioOuterBar'><div class='audioInnerBarL' id='"+channelDesc+"_audioL'></div><div class='audioInnerBarR' id='"+channelDesc+"_audioR'></div></div>";
				} else {
					html += "<tr><td>Quad split</td>";
				}
				html += "<td id='"+channelDesc+"_recSettings'></td>";
				html += "</tr>";
			}
			html += "</table></div>";
		}
		// Draw to screen
		this.container.$().innerHTML = html;
		
		// Store the IDs of the relevant DOM elements for updating later
		for (var recorder in data) {
			for (channel in data[recorder]) {
				var channelDesc = 'field_'+this.monitor.name+"_"+this.instance.name+"_"+recorder+"_"+channel;
				this.elements[recorder][channel].image = channelDesc + "_img";
				this.elements[recorder][channel].recSettings = channelDesc + "_recSettings";
				if(channel != "quad"){
					this.elements[recorder][channel].timecode = channelDesc + "_timecode";
					this.elements[recorder][channel].audioL = channelDesc + "_audioL";
					this.elements[recorder][channel].audioR = channelDesc + "_audioR";
					this.elements[recorder][channel].state = channelDesc + "_state";
				}
			}
		}
	}

	var errorCode = "ok";
	var overallCaptureError = "ok";
	// If no recorders, we need to say so, and stop updating any which were previously displayed
	if(data.noRecorders == true && this.showingNoRecorders != true){
		// Remove all timecodes incrementing as they have no meaning anymore!
		for (var timecode in this.timecodes) {
			this.timecodes[timecode].remove();
		}
		if(visible) {
			this.container.$().innerHTML = "<p style='font-weight:bold;'>No recorders configured for this machine. Input channels can be monitored on the <a href='javascript:getContent(\"index\",false,\"Status_Capture\",\"index::::Status_Capture\")'>capture</a> page.</p>";
			this.showingNoRecorders = true;
		}
		if (!this.warnedNoRecorders) {
			insole.warn('No recorders running on '+this.instance.name);
			this.warnedNoRecorders = true;
		}
	} else if (data.noRecorders != true){
	// If there are recorders, iterate over them
		this.warnedNoRecorders = false;
		for (var recorder in data) {
			var rec = data[recorder];
			//for each channel in the recorder
			for (var channel in rec){
				var channelDesc = 'field_'+this.monitor.name+"_"+this.instance.name+"_"+recorder+"_"+channel;
				var captureData = instanceData.monitorData.ring[channel];
				// recording info
				var recordingError = Status_processRecordingInfo(this,instanceData,recorder,channel,visible);
				if(recordingError) errorCode = "error";
				if (channel != "quad") {
					// input info
					var captureError = Status_processCaptureChannel(this,channel,captureData,recorder,recordingError,visible);
					if (captureError == "error") {
						overallCaptureError = "error";
					} else if (overallCaptureError == "ok" && captureError == "warn") {
						overallCaptureError = "warn"
					}
				} else if (visible) {
					// Because the overall channel status icon for the capture channel is not set by the capture channel info
					// (because there is no associated capture channel), set it based on record status for the quad split
					if (!recordingError) {
						this.elements[recorder][channel].image.$().src = this.monitor.myParent.image.ok.src;
					} else {
						this.elements[recorder][channel].image.$().src = this.monitor.myParent.image.error.src;
						errorCode = "error";
					}
				}
			}
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
	
	if (overallCaptureError == "error") {
		errorCode = "error";
	} else if (errorCode == "ok" && overallCaptureError == "warn") {
		errorCode = "warn"
	}
	
	return errorCode;
}
typeHandlers.recorderStateList = Status_process_recorderState;

// Draw recording info for a particular channel
function Status_processRecordingInfo (field,instanceData,recorder,channel,visible) {
	var channelDesc = 'field_'+field.monitor.name+"_"+field.instance.name+"_"+recorder+"_"+channel;
	var recData = instanceData.monitorData[field.name][recorder][channel];
	var elements = false;
	
	if(visible) {
		elements = field.elements[recorder][channel];
		// Has the number of recordings changed? If so, we'll need to redraw
		var forceRedraw = false;
		var last = field.instance.getHistory(1);
		var current = field.instance.getHistory(0);
		if(last&& last.monitorData[field.name][recorder][channel]) {
			var numInLast = 0;
			for (var r in last.monitorData[field.name][recorder][channel]) { numInLast++; }
			var numInCurrent = 0;
			for (var r in current.monitorData[field.name][recorder][channel]) { numInCurrent++; }
			if(numInLast != numInCurrent) {
				forceRedraw = true;
			}
		}
	
		// If not drawn, draw the layout
		if(forceRedraw || elements.recSettings.$().innerHTML == "" || (elements.noEncoders == true && recData.noEncoders != true)){
			if(recData.noEncoders == true) {
				elements.recSettings.$().innerHTML = "No encodings configured for this channel";
				elements.noEncoders = true;
			} else {
				var html = "";
				for(recording in recData){
					html += "<table><tr><td>";
					html += "<img class='recorderChanImg' id='"+channelDesc+"_"+recording+"_errimg' src='/ingex/img/error_small.gif'/>";
					html += "<img class='recorderChanImg' id='"+channelDesc+"_"+recording+"_recimg' src='/ingex/img/recording.gif'/>";
					html += '</td><td><div class="bufferOuterBar">';
					html += '<div id="'+channelDesc+"_"+recording+'_bufferBar" class="bufferInnerBar">';
					html += '<span class="bufferLabel">Buffer --%</span></div></div></td>';
					html += '</div></td>';
					html += "<td>Format: <span id='"+channelDesc+"_"+recording+"_encType'></span><br />";
					html += "<span id='"+channelDesc+"_"+recording+"_state'></span></td></table>";
					elements[recording] = new Object();
					elements[recording].errimage = channelDesc+"_"+recording+"_errimg";
					elements[recording].recimage = channelDesc+"_"+recording+"_recimg";
					elements[recording].state = channelDesc+"_"+recording+"_state";
					elements[recording].bufferBar = channelDesc+"_"+recording+'_bufferBar';
					elements[recording].encType = channelDesc+"_"+recording+'_encType';
				}
				elements.recSettings.$().innerHTML = html;
				elements.noEncoders = false;
			}
		}
	}

	// Show the actual data
	if(elements == false || elements.noEncoders == false){
		var stateMessage;
		var bufferMessage;
		var recVisible = false;
		var errVisible = false;
		var recordingError = false;
		for (var recording in recData) {
			// for each recording type
			if(recData[recording].record_error){
				errVisible = true;
				recVisible = false;
				stateMessage = "<span class='error'>"+recData[recording].error+"</span> ";
				recordingError = true;
				errorCode = "error";
				logChange(field,'alert',null,recorder,channel,recording,'record_error');
			} else if (recData[recording].recording){
				errVisible = false;
				recVisible = true;
				stateMessage = "";
				logChange(field,null,'Recording started',recorder,channel,recording,'recording');
			} else {
				errVisible = false;
				recVisible = false;
				stateMessage = "";
				logChange(field,null,'Recording stopped',recorder,channel,recording,'recording');
			}
			
			// Work out the percentage used (either from amount used or amount free data)
			var used = Math.round(recData[recording].frames_in_backlog/instanceData.monitorData.ringlen)
			if(visible) {
				// Set the bar's width
				bar = elements[recording].bufferBar.$();
				bar.style.width = (used) + 'px';
				bar.firstChild.innerHTML = 'Buffer '+used+'% used';
			}
			if(used >= field.monitor.thresholds.buffer) {
				if(visible) bar.style.backgroundColor = '#f00';
				
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
				if(log) insole.alert("Recording buffer at dangerous level for type "+recording+" on channel "+channel+" of recorder "+recorder+" on "+field.instance.name);
				
			} else {
				var log = false;
				if(typeof field.instance.getHistory(1) != "undefined" ){
					// if there is previous history data...
					if (typeof field.instance.getHistory(1).monitorData != "undefined" && typeof field.instance.getHistory(1).monitorData[field.name] != "undefined" && field.instance.getHistory(1).monitorData[field.name].thresholdBroken == true) {
						// if the threshold wasn broken last time, inform about it being ok now
						log = true;
					}
				}
				if(log) insole.log("Recording buffer now at safe level for type "+recording+" on channel "+channel+" of recorder "+recorder+" on "+field.instance.name);
				
				if(visible) {
					if (used == 0){
						bar.style.backgroundColor = 'transparent';
					} else {
						bar.style.backgroundColor = '#999';
					}
				}
			}
			
			if(visible) {
				// Update the display
				if(recVisible) {
					elements[recording].recimage.$().style.display = 'inline';
				} else {
					elements[recording].recimage.$().style.display = 'none';
				}
				if(errVisible) {
					elements[recording].errimage.$().style.display = 'inline';
				} else {
					elements[recording].errimage.$().style.display = 'none';
				}
				elements[recording].state.$().innerHTML = stateMessage;
				elements[recording].encType.$().innerHTML = recData[recording].desc;
			}
		}
	}
	return recordingError;
}

var onLoad_Status_Recorders = function () {
	Status_loadMonitor('Recorders');
}
loaderFunctions.Status_Recorders = onLoad_Status_Recorders;