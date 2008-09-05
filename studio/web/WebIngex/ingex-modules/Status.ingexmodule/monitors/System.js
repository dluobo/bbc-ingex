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

// --- System Monitor Javascript (part of Status module) ---

// Display volume information (free disk space)
function Status_process_volumes (instanceData,visible) {
	// We're going to call the generic usageBar function, but need to specify some info 
	// first on how to handle the data we're passing
	var dataDescription = {
		total : "total",
		free : "free",
		used : null,
		totalValue : null,
		multiItemData : true
	};
	// Filter the volumes to just the ones we want to monitor (according to the config)
	if(this.instance.volumes != "") {
		// If Array doesn't have an indexOf method (because, say, you're using a useless browser... *cough* IE *cough*) then create one
		if (typeof Array.prototype.indexOf == "undefined"){
			Array.prototype.indexOf = function(x) {
				for (var i=0; i < this.length; i++) {
					if (this[i] == x) {
						return i;
					}
				}
				return -1;
			};
		}
		for(volume in instanceData.monitorData[this.name]) {
			// If volume is not in this instance's volumes list...
			if(this.instance.volumes.indexOf(volume) == -1) {
				// remove its data
				instanceData.monitorData[this.name][volume] = false;
			}
		}
	}
	return Status_process_usageBar(instanceData,visible,dataDescription,this.monitor.thresholds.volumes,"Volumes:",null,true,this);
}
typeHandlers.volumes = Status_process_volumes;

// Display CPU usage
function Status_process_cpu (instanceData, visible) {
	// We're going to call the generic usageBar function, but need to specify some info 
	// first on how to handle the data we're passing
	var dataDescription = {
		total : null,
		free : "idle",
		used : null,
		totalValue : 100,
		multiItemData : false
	};
	return Status_process_usageBar(instanceData,visible,dataDescription,this.monitor.thresholds.cpu,"CPU Usage:","transparent",false,this);
}
typeHandlers.cpu = Status_process_cpu;

// Display a generic usage bar (called directly for memory, or by one of the above functions for volumes and cpu)
function Status_process_usageBar (instanceData,visible,dataDescription,threshold,label,bgCol,showInfoLine,field) {
	if (isDefault(field)) field = this;
	var data = instanceData.monitorData[field.name];
	if (isDefault(label)) label = "Memory:";
	if (isDefault(dataDescription)) dataDescription = {used:false,free:"free",total:"total",multiItemData:false};
	if (isDefault(threshold)) threshold = field.monitor.thresholds.memory;
	var threshOver = true;
	if (threshold < 0){
		threshold = -threshold;
		threshOver = false;
	}
	if (isDefault(showInfoLine)) showInfoLine = true;
	if (!isDefault(dataDescription.totalValue)) {
		dataDescription.total = "total";
		data.total = dataDescription.totalValue;
	}
	var errorLevel = 0;

	if (visible) {	
		// If not drawn, draw the data layout
		if(field.container.$().innerHTML == ''){
			var html = "";
			if(label) html += "<p style='font-weight:bold;'>"+label+"</p>";
			html += "<table class='usageTable'>";
			if (dataDescription.multiItemData) {
				for (var item in data) {
					// Check that this data item hasn't been disabled
					if(data[item] != false) {
						html += '<tr><td class="statusOuterBar"';
						if(bgCol) html += 'style="background-color:'+bgCol+';';
						html += '">';
						html += '<div class="statusInnerBar" id="usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_'+item+'">'+item+'</div></td>';
						if(showInfoLine) html += '<td class="statusInfoLine" id="usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_'+item+'_info"></td>';
						html += '</tr>';
					}
				}
			} else {
				html += '<tr><td class="statusOuterBar"';
				if(bgCol) html += 'style="background-color:'+bgCol+';';
				html += '">';
				html += '<div id="usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'" class="statusInnerBar">';
				html += '<span style="padding-left:3px;">--%</span></div></td>';
				if(showInfoLine) html += '<td class="statusInfoLine" id="usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_info"></td>';
				html += '</tr>';
			}
			html += '</table>';
			field.container.$().innerHTML = html;
			if(dataDescription.multiItemData) {
				for(var item in data) {
					// Check that this data item hasn't been disabled
					if(data[item] != false) {
						field.elements[item] = new Object();
						field.elements[item].usagebar = 'usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_'+item;
						if(showInfoLine) field.elements[item].infoline = 'usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_'+item+'_info';
					}
				}
			} else {
				field.elements.usagebar = 'usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name;
				if(showInfoLine) field.elements.infoline = 'usagebar_'+field.monitor.name+'_'+field.instance.name+'_'+field.name+'_info';
			}
		}
	}
	
	// Draw the data
	var used = 0;
	var bar;
	var thresholdBroken;
	if(dataDescription.multiItemData) {
		// If there's more than one bar to draw (e.g. for volumes) loop over them
		for (var item in data) {
			// Check that this data item hasn't been disabled
			if(data[item] != false) {
				// Work out the percentage used (either from amount used or amount free data)
				if(dataDescription.used) {
					used = Math.round(100*(data[item][dataDescription.used]/data[item][dataDescription.total]));
				} else if(dataDescription.free) {
					used = 100-Math.round(100*(data[item][dataDescription.free]/data[item][dataDescription.total]));
				}
				
				thresholdBroken = false;
				if(threshOver && used >= threshold) {
					thresholdBroken = true;
					errorLevel++;
				} else if (!threshOver && data[item][dataDescription.free] <= threshold) {
					thresholdBroken = true;
					errorLevel++;
				}
				// Colour the bar red if the value is over/under the threshold (as appropriate)
				var bgCol
				if(thresholdBroken){
					bgCol = '#f00';
					
					var log = false;
					if(typeof field.instance.getHistory(0) != "undefined"){
						// if there's a history entry for the current data (there should be!) log the broken threshold
						field.instance.getHistory(0).monitorData[field.name][item].thresholdBroken = true;
					}
					if(typeof field.instance.getHistory(1) != "undefined" ){
						// if there is previous history data...
						if (typeof field.instance.getHistory(1).monitorData[field.name][item].thresholdBroken == "undefined") {
							// if the threshold wasn't broken last time, warn about it being broken now
							log = true;
						}
					} else {
						// if there is no previous history data, we have probably just loaded the page, so warn about the broken threshold
						log = true;
					}
					if(log) insole.warn(field.monitor.friendlyName+" on "+field.instance.name+": Threshold broken for "+label+" - "+item+" (threshold: "+threshold+"%, value: "+used+"%)");
				} else {
					if(typeof field.instance.getHistory(1) != "undefined" && typeof field.instance.getHistory(1).monitorData != "undefined"){
						// if there is previous history data...
						if (field.instance.getHistory(1).monitorData[field.name][item].thresholdBroken == true) {
							// if the threshold was broken last time, inform about it being ok now
							insole.log(field.monitor.friendlyName+" on "+field.instance.name+": Value now under threshold for "+label+" - "+item+" (threshold: "+threshold+"%, value: "+used+"%)");
						}
					}
					if (used == 0){
						bgCol = 'transparent';
					} else {
						bgCol = '#999';
					}
				}
				// Set the bar's width
				if (visible) {
					bar = field.elements[item].usagebar.$();
					bar.style.width = (6*used)+'px';
					bar.style.backgroundColor = bgCol;
				
					// Display textual data if requested
					if(showInfoLine) {
						info = field.elements[item].infoline.$();
						info.innerHTML = 'Free: '+(100-used)+'% of '+Math.round(10*(data[item][dataDescription.total]/1024/1024/1024))/10+' GB';
					}
				}
			}
		}
	} else {
		// Work out the percentage used (either from amount used or amount free data)
		if(dataDescription.used) {
			used = Math.round(100*(data[dataDescription.used]/data[dataDescription.total]));
		} else if(dataDescription.free) {
			used = 100-Math.round(100*(data[dataDescription.free]/data[dataDescription.total]));
		}
		
		thresholdBroken = false;
		if(threshOver && used >= threshold) {
			thresholdBroken = true;
			errorLevel++;
		} else if (!threshOver && data[dataDescription.free] <= threshold) {
			thresholdBroken = true;
			errorLevel++;
		}

		// Colour the bar red if the value is over/under the threshold (as appropriate)
		var bgCol;
		if(thresholdBroken){
			bgCol = '#f00';
			
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
			if(log) insole.warn(field.monitor.friendlyName+" on "+field.instance.name+": Threshold broken for "+label+" (threshold: "+threshold+"%, value: "+used+"%)");
			
		} else {
			if(typeof field.instance.getHistory(1) != "undefined" && typeof field.instance.getHistory(1).monitorData != "undefined"){
				// if there is previous history data...
				if (field.instance.getHistory(1).monitorData[field.name].thresholdBroken == true) {
					// if the threshold was broken last time, inform about it being ok now
					insole.log(field.monitor.friendlyName+" on "+field.instance.name+": Value now under threshold for "+label+" (threshold: "+threshold+"%, value: "+used+"%)");
				}
			}
			if (used == 0){
				bgCol = 'transparent';
			} else {
				bgCol = '#999';
			}
		}
		if (visible) {
			// Set the bar's width
			bar = field.elements.usagebar.$();
			bar.style.width = (6*used)+'px';
			bar.firstChild.innerHTML = used+'%';
			bar.style.backgroundColor = bgCol;
		
			// Display textual data if requested
			if(showInfoLine) {
				info = field.elements.infoline.$();
				info.innerHTML = 'Free: '+(100-used)+'% of '+Math.round(10*(data[dataDescription.total]/1024/1024/1024))/10+' GB';
			}
		}
		
		if(typeof monitors.biggestMemIncrease == "undefined") monitors.biggestMemIncrease = 0;
		if(label == "Memory:") {
			var lastData = field.instance.getHistory();
			if(lastData && typeof lastData.monitorData != "undefined") {
				if(dataDescription.used) {
					lastUsed = Math.round(100*(lastData.monitorData[field.name][dataDescription.used]/lastData.monitorData[field.name][dataDescription.total]));
				} else if(dataDescription.free) {
					lastUsed = 100-Math.round(100*(lastData.monitorData[field.name][dataDescription.free]/lastData.monitorData[field.name][dataDescription.total]));
				}
				if(logMemoryUsageChanges) {
					var change = (used-lastUsed);
					if(change < 0) change = -change;
					var biggest = monitors.biggestMemIncrease;
					if(biggest < 0) biggest = -biggest;
					if(change > biggest) {
						insole.log("MEM: Increased by "+(used-lastUsed),"alert");
						monitors.biggestMemIncrease = (used-lastUsed);
					}
				}
			}
		}
	}
	if(errorLevel > 0) {
		monitors.osh.warn(field);
		return "warn";
	}
	monitors.osh.ok(field);
	return "ok";
}
typeHandlers.usageBar = Status_process_usageBar;

var onLoad_Status_System = function () {
	Status_loadMonitor('System');
}
loaderFunctions.Status_System = onLoad_Status_System;