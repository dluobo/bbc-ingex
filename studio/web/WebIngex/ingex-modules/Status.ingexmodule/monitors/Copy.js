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

// --- Copy Monitor Javascript (part of Status module) ---

function Status_process_copyProgress (instanceData,visible) {
	// We're going to call the generic usageBar function, but need to specify some info 
	// first on how to handle the data we're passing
	var dataDescription = {
		total : null,
		free : null,
		used : 'fileProgress',
		totalValue : 100,
		multiItemData : false
	};
	
	// If switching to multiple progress meters (total & current file for example) I suggest looking inti
	// turning multiItemData on. Then change the label to just "Progress:" and...maybe... you should get 2
	// independently labelled progress bars (see the volumes section of the System monitor for an example)
	// Data would need to look like:
	//		progress : {  current:{done:20} , total:{done:40}  }
	// and the 'used' value in the data description changed to 'done' (in the case shown here)
	
	var errorCode = Status_process_usageBar(instanceData,visible,dataDescription,null,"Current File Progress:","transparent",false,this);
	
	return errorCode;
}
typeHandlers.copyProgress = Status_process_copyProgress;

function Status_process_copyRecs (instanceData, visible) {
	var data = instanceData.monitorData[this.name];
	
	var speed_msg;
	if(data.length == 0){
		speed_msg = "Fast (No recordings active)";
	} else {
		speed_msg = "Slow (Speed limited due to active recordings)";
	}
	
	// If not drawm, draw the layout
	if(visible && this.container.$().innerHTML == ''){
		var h = "<span style='font-weight:bold;'>Copy Speed : </span>";
		h += "<span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_speed'></span>";
		this.container.$().innerHTML = h;
		this.elements.speed = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_speed";
	}

	if(visible){
	// Fill in the data
	this.elements.speed.$().innerHTML = speed_msg;
	}
}
typeHandlers.copyRecs = Status_process_copyRecs;

function Status_process_copyCurrent (instanceData, visible) {
	var data = instanceData.monitorData[this.name];

	if(visible){
		// If not drawm, draw the layout
		if(this.container.$().innerHTML == ''){
			var h = "<div style='font-weight:bold;'>Current Copy Priority : </div>";
			h += "<div class='copyIndentedBlock'>Priority: <span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_priority'></span></div>";
			h += "<div class='copyIndentedBlock'>Total files: <span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_totalfiles'></span></div>";
			h += "<div class='copyIndentedBlock'>Total size: <span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_totalsize'></span>MB</div>";
			h += "<div style='font-weight:bold;'>Current Copy File : </div>";
			h += "<div class='copyIndentedBlock'>Filename: <span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_filename'></span></div>";
			h += "<div class='copyIndentedBlock'>File size: <span id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_filesize'></span>MB</div>";
			this.container.$().innerHTML = h;
			this.elements.priority = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_priority";
			this.elements.totalfiles = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_totalfiles";
			this.elements.totalsize = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_totalsize";
			this.elements.currentfilename = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_filename";
			this.elements.currentfilesize = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_filesize";
		}
	
		// Fill in the data
		if(!data.priority) { data.priority = ""; }
		if(!data.totalFiles) { data.totalFiles = 0; }
		if(!data.totalSize) { data.totalSize = 0; }
		if(!data.fileName) { data.fileName = "(No file)"; }
		if(!data.fileSize) { data.fileSize = 0; }
		this.elements.priority.$().innerHTML = data.priority;
		this.elements.totalfiles.$().innerHTML = data.totalFiles;
		this.elements.totalsize.$().innerHTML = Math.round(data.totalSize/1024);
		this.elements.currentfilename.$().innerHTML = data.fileName;
		this.elements.currentfilesize.$().innerHTML = Math.round(data.fileSize/1024);
	}
}
typeHandlers.copyCurrent = Status_process_copyCurrent;

function Status_process_copyEndpoints (instanceData, visible) {
	var data = instanceData.monitorData[this.name];

	if(visible){
		if(this.container.$().innerHTML == "") {
			var h = "<span style='font-weight:bold;'>Copy End Points:</span><br />";
			this.container.$().innerHTML = h;
		}
		
		if (typeof this.priorities == "undefined") this.priorities = new Object();
		
		var redrawPriorities = false;
		for (var i in data) {
			if (typeof data[i] != "function") {
				if (typeof this.priorities[i] == "undefined") {
					this.priorities[i] = new Object();
					redrawPriorities = true;
				}
			}
		}
		
		for (var i in this.priorities) {
			if(redrawPriorities) {
				this.container.$().innerHTML += "<div id='field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_priority'></div>";
				this.elements[i] = "field_"+this.monitor.name+"_"+this.instance.name+"_"+this.name+"_priority";
			}
			var x = "";
			for (var j in data[i]) {
				if(typeof data[i][j] != "function") {
					x += "<div class='copyIndentedBlock'>"+data[i][j].source+" => "+data[i][j].destination+"</div>";
				}
			}
			this.elements[i].$().innerHTML = "Priority "+i+":"+x;
		}
	}
}
typeHandlers.copyEndpoints = Status_process_copyEndpoints;

var onLoad_Status_Copy = function () {
	Status_loadMonitor('Copy');
}
loaderFunctions.Status_Copy = onLoad_Status_Copy;