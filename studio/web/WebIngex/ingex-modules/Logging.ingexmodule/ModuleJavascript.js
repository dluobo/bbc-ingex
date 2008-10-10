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

// --- Logging Module Javascript ---

// --- Options ---
// queryLoc defines the location to query for a timecode. e.g. if host name for the recorder is 192.168.1.10 and
// queryLoc = ":7000/tc" then timecode requests will be made to 192.168.1.10:7000/tc
var queryLoc = ":7000/tc";
// Frame rate
var editrate = 25;


var tree = false;
var treeLoader = false;
var onLoad_Logging = function() {
	checkJSLoaded(function(){Logging_init();});
}
loaderFunctions.Logging = onLoad_Logging;

var ILtc = false;

/// Initialise the interface, set up the tree, the editors etc, plus the KeyMap to enable keyboard shortcuts
function Logging_init () {
	treeLoader = new Ext.tree.TreeLoader({
		dataUrl : "/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl",
		uiProviders:{
			'take': Ext.tree.TakeNodeUI,
			'item': Ext.tree.ItemNodeUI
		}
	});
	
	treeLoader.on("beforeload", function() {
			if($('programmeSelector').selectedIndex != -1) {
	        	treeLoader.baseParams.progid = $('programmeSelector').options[$('programmeSelector').selectedIndex].value;
			} else {
				treeLoader.baseParams.progid = -1;
			}
	    });
	
	tree = new Ext.tree.ColumnTree({
		width: '100%',
		height: 300,
		rootVisible:false,
		autoScroll:true,
		enableDD:true,
		ddAppendOnly: true,
		// title: 'Takes',
		renderTo: 'takesList',
		
		columns:[{
			header:'Take Num.',
			width:80,
			dataIndex:'takeNo'
		},{
			header:'Location',
			width:100,
			dataIndex:'location'
		},{
			header:'Date',
			width:80,
			dataIndex:'date'
		},{
			header:'In',
			width:80,
			dataIndex:'inpoint'
		},{
			header:'Out',
			width:80,
			dataIndex:'out'
		},{
			header:'Duration',
			width:80,
			dataIndex:'duration'
		},{
			header:'Result',
			width:80,
			dataIndex:'result'
		},{
			header:'Comment',
			width:220,
			dataIndex:'comment'
		}],

		loader: treeLoader,
		
		root: new Ext.tree.AsyncTreeNode({
			id: 0
		})
	});

	var treeSorter = new Ext.tree.TreeSorter(tree, {
		sortType: ILgetFramesTotal
	});
	
	// render the tree
	tree.render();
	tree.expandAll();
	
	treeEditor = new Ext.tree.ColumnTreeEditor(tree,{ 
		ignoreNoChange: true,
		editDelay: 0
	});
	
	tree.on("valuechange", treeSorter.updateSortParent, treeSorter);
	tree.on("valuechange", ILpopulateItemList);
	tree.on("append", ILpopulateItemListNew);
	tree.on("remove", ILpopulateItemList);
	tree.on("valuechange", ILupdateTakeNumbering);
	tree.on("append", ILupdateTakeNumbering);
	tree.on("remove", ILupdateTakeNumbering);
	tree.on("valuechange", ILupdateNodeInDB);
	
	ILpopulateItemList();
	
	Ext.getDoc().addKeyMap([
		{
			key: Ext.EventObject.ENTER,
			stopEvent: true,
			ctrl: true,
			fn: ILstoreTake
		}, {
			key: Ext.EventObject.ESC,
			stopEvent: true,
			fn: ILresetTake
		}, {
			key: Ext.EventObject.UP,
			stopEvent: true,
			ctrl: true,
			fn: function(){ 
				if(tree.getSelectionModel().getSelectedNode()) {
					tree.getSelectionModel().selectPrevious(); 
				} else {
					var lastItem = tree.getRootNode().lastChild;
					if(lastItem.lastChild != null) {
						tree.getSelectionModel().select(lastItem.lastChild);
					} else {
						tree.getSelectionModel().select(lastItem);
					}
				}
			}
		}, {
			key: Ext.EventObject.DOWN,
			stopEvent: true,
			ctrl: true,
			fn: function(){ 
				if(tree.getSelectionModel().getSelectedNode()) {
					tree.getSelectionModel().selectNext(); 
				} else {
					tree.getSelectionModel().select(tree.getRootNode().firstChild);
				}
			}
		}, {
			key: Ext.EventObject.RIGHT,
			fn: ILselectNextItem,
			ctrl: true,
			stopEvent: true
		}, {
			key: Ext.EventObject.LEFT,
			fn: ILselectPrevItem,
			ctrl: true,
			stopEvent: true
		}, {
			key: Ext.EventObject.NUM_PLUS,
			fn: ILsetGood,
			stopEvent: true
		}, {
			key: Ext.EventObject.NUM_MINUS,
			fn: ILsetNoGood,
			stopEvent: true
		}, {
			key: Ext.EventObject.NUM_MULTIPLY,
			fn: ILstartStop,
			stopEvent: true
		}, {
			key: Ext.EventObject.DELETE,
			fn: ILdeleteNode,
			stopEvent: true
		}, {
			key: Ext.EventObject.N,
			ctrl: true,
			fn: function(){ILnewNode();}, //avoid passing unexpected parameters by wrapping the call in an anonymous function
			stopEvent: true
		}, {
			key: [Ext.EventObject.P,Ext.EventObject.D],
			ctrl: true,
			fn: ILduplicateNode,
			stopEvent: true
		}, {
			key: Ext.EventObject.C,
			ctrl: true,
			shift: true,
			fn: function() {$('commentBox').focus();},
			stopEvent: true
		}
	]);

	ILdiscoverRecorders();
	ILpopulateProgRecInfo();

	ILtc = new ingexLoggingTimecode();
}

/// Start/stop take timecode updates
function ILstartStop () {
	if(!ILtc.takeRunning) {
		// START
		$('startStopButton').innerHTML = 'Stop';
		ILtc.startTake();
	} else {
		// STOP
		$('startStopButton').innerHTML = 'Start';
		ILtc.stopTake();
	}
}

/// Examine a node's inpoint and convert it to a number of frames
/// @param node the node to examine
/// @return the number of frames
function ILgetFramesTotal (node) {
	if(typeof node.attributes.inpoint != "undefined") {
		var chunks = node.attributes.inpoint.split(":");
		var h = parseInt(chunks[0], 10);
		var m = parseInt(chunks[1], 10);
		var s = parseInt(chunks[2], 10);
		var f = parseInt(chunks[3], 10);
		return f+(editrate*s)+(60*editrate*m)+(60*60*editrate*h);
	} else {
		return -1;
	}
}

/// Convert a textual timecode string (aa:bb:cc:dd) to a number of frames
/// @param text the timecode string
/// @return the number of frames
function ILgetFramesTotalFromText (text) {
	var chunks = text.split(":");
	var h = parseInt(chunks[0], 10);
	var m = parseInt(chunks[1], 10);
	var s = parseInt(chunks[2], 10);
	var f = parseInt(chunks[3], 10);
	return f+(editrate*s)+(60*editrate*m)+(60*60*editrate*h);
}

/// Convert a number of frames to a timecode string
/// @param length the number of frames
/// @return the timecode string
function ILgetTextFromFramesTotal (length) {
	var h; var m; var s; var f;
	s = Math.floor(length/editrate);
	m = Math.floor(s/60);
	h = Math.floor(m/60);
	f = length - (editrate * s);
	s = s - (60 * m);
	m = m - (60 * h);
	
	if (h < 10) { h = "0" + h; }
	if (m < 10) { m = "0" + m; }
	if (s < 10) { s = "0" + s; }
	if (f < 10) { f = "0" + f; }
	
	return h+":"+m+":"+s+":"+f;
}

/// Update the take numbering in the tree (called when a take is moved)
function ILupdateTakeNumbering () {
	var items = tree.getRootNode().childNodes;
	for (var i in items) {
		var item = items[i];
		if(typeof item != "object") continue;
		var childNodes = item.childNodes;
		childNodes.sort(function(a,b){
			var ft_a = ILgetFramesTotal(a);
			var ft_b = ILgetFramesTotal(b);
			return ft_a - ft_b;
		});
		var tn = 0;
		for (var take in childNodes) {
			if(typeof childNodes[take].attributes != "undefined") {
				tn++;
				if (childNodes[take].attributes.takeNo != tn){
					childNodes[take].setText(tn);
					childNodes[take].attributes.takeNo = tn;
					ILupdateNodeInDB(childNodes[take],'takeNo',tn);
					ILupdateNodeInDB(childNodes[take],'item',items[i].attributes.databaseID);
				}
			}
		}
	}
}

/// Create a new item
/// @param value the item name
/// @param seq the sequence reference string
function ILnewNode (value, seq) {
	var newItemName;
	if (isDefault(value)) {
		value = '';
		newItemName = '[new item]';
	} else {
		newItemName = value;
	}
	
	var sequenceName;
	if (isDefault(seq)) {
		seq = "{}";
		sequenceName = "[no sequence]";
	} else {
		sequenceName = seq;
		seq = "{"+seq+"}";
	}
	
	var newID = -1;
	var children = tree.getRootNode().childNodes;
	for (var child in children) {
		if(child != "remove") {
			var id = children[child].id.substring(5);
			if(id > newID) newID = id;
		}
	}
	newID++;
	
	var tmpNode= new Ext.tree.TreeNode({
		id: "item-"+newID,
		itemName: value,
		sequence: sequenceName,
		uiProvider: Ext.tree.ItemNodeUI
	});
	var newNode;
	if(tree) { 
		newNode = tree.getRootNode().appendChild(tmpNode);
		var el = newNode.getUI().getItemNameEl();
		treeEditor.completeEdit();
		treeEditor.editNode = newNode;
		treeEditor.targetElement = el;
		treeEditor.startEdit(el,value);
	}
	
	// -- Send to database
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addItem.pl',
		params : {
			id: newID,
			name: newItemName,
			sequence: seq,
			programme: $('programmeSelector').options[$('programmeSelector').selectedIndex].value
		},
		success: function(response) {
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.log("JSON parse error: "+e.name+" - "+e.message);
				insole.log("JSON data was: "+response.responseText);
				insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
			}
			if(data.success) {
				newNode.attributes.databaseID = data.id;
				insole.log("Successfully added item to database: "+newItemName);
			} else {
				insole.error("Error adding to database: "+data.error);
				insole.alert("Failed to add item to database: "+newItemName+". See previous log message.");
			}
		},
		failure: function() {
			insole.alert("Failed to add item to database: "+newItemName+". Recommend you refresh this page to check data integrity.");
		}
	});
}

/// Duplicate the currently selected item
function ILduplicateNode () {
	if(tree && tree.getSelectionModel().getSelectedNode()) {
		var value = "";
		var seq = "";
		if(tree.getSelectionModel().getSelectedNode().attributes.itemName) {
			value = tree.getSelectionModel().getSelectedNode().attributes.itemName;
			seq = tree.getSelectionModel().getSelectedNode().attributes.sequence;
		} else if (tree.getSelectionModel().getSelectedNode().parentNode && tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName){
			value = tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName;
			seq = tree.getSelectionModel().getSelectedNode().parentNode.attributes.sequence;
		}
		ILnewNode("[Pickup] "+value,seq);
	}
}

/// Delete the currently selected item
function ILdeleteNode () {
	if(tree && tree.getSelectionModel().getSelectedNode()) {
		var node = tree.getSelectionModel().getSelectedNode();
		Ext.MessageBox.confirm('Confirm Deletion', 'Are you sure you wish to delete this item and all its takes? You cannot undo this.', function(response){
			if(response == "yes") {
				var node = false;
				if(tree.getSelectionModel().getSelectedNode().attributes.itemName) {
					node = tree.getSelectionModel().getSelectedNode();
				} else if (tree.getSelectionModel().getSelectedNode().parentNode.attributes.itemName){
					node = tree.getSelectionModel().getSelectedNode().parentNode;
				}

				// -- Remove from database
				Ext.Ajax.request({
					url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/deleteItem.pl',
					params : {
						id: node.attributes.databaseID
					},
					success: function(response) {
						try {
							var data = JSON.parse(response.responseText);
						} catch (e) {
							insole.log("JSON parse error: "+e.name+" - "+e.message);
							insole.log("JSON data was: "+response.responseText);
							insole.alert("Failed to delete item from database: "+node.attributes.itemName+". See previous log message.");
						}
						if(data.success) {
							insole.log("Successfully deleted item from database: "+node.attributes.itemName);
						} else {
							insole.error("Error adding to database: "+data.error);
							insole.alert("Failed to deleting item from database: "+node.attributes.itemName+". See previous log message.");
						}
					},
					failure: function() {
						insole.alert("Failed to delete item from database: "+node.attributes.itemName+". Recommend you refresh this page to check data integrity.");
					}
				});
				
				// -- Remove from tree
				if(node) node.remove();
			}
		});
	}
}

/// Store the new take
function ILstoreTake () {
	var locSel = $("recLocSelector");
	var itemSel = $('itemSelector');
	try {
		var item = tree.getNodeById(itemSel.options[itemSel.selectedIndex].value);
	} catch (e) {
		insole.alert("Error getting parent Item for new take: "+e.name+" - "+e.message);
	}
	var takeNumber;
	if(item.lastChild) {
		takeNumber = item.lastChild.attributes.takeNo + 1;
	} else {
		takeNumber = 1;
	}
	var now = new Date();
	var dd = now.getDate();
	var mm = now.getMonth() + 1; // it ranges 0-11 so add 1
	var yy = now.getFullYear();
	if (dd < 10) { dd = "0" + dd; }
	if (mm < 10) { mm = "0" + mm; }
	var sqlDate = yy+"-"+mm+"-"+dd;
	
	var locationName = locSel.options[locSel.selectedIndex].innerHTML;
	var locationID = locSel.options[locSel.selectedIndex].value;
	var result = $('resultText').innerHTML;
	var comment = $('commentBox').value;
	if (comment == "") comment  = "[no data]";
	
	var inpoint = $('inpointBox').value;
	var outpoint = $('outpointBox').value;
	var duration = $('durationBox').value;
	var start = ILgetFramesTotalFromText(inpoint);
	var length = ILgetFramesTotalFromText(duration);
	
	// -- Create node
	var tmpNode= new Ext.tree.TreeNode({
		takeNo: takeNumber,
		location: locationName,
		date: sqlDate,
		inpoint: inpoint,
		out: outpoint,
		duration: duration,
		result: result,
		comment: comment,
		leaf: true,
		uiProvider: Ext.tree.TakeNodeUI
	});
	
	// -- Add to tree
	if(tree) {
		try {
			var newNode = item.appendChild(tmpNode);
			item.expand();
			ILresetTake();
		} catch (e) {
			insole.alert("Error storing new take: "+e.name+" - "+e.message);
		}
	} else {
		insole.alert("Error storing new take: tree unavailable.");
	}

	// -- Send to database
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/data/addTake.pl',
		params : {
			takeno: takeNumber,
			location: locationID,
			date: sqlDate,
			start: start,
			length: length,
			result: result,
			comment: comment,
			item: item.attributes.databaseID,
			editrate: editrate
		},
		success: function(response) {
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.log("JSON parse error: "+e.name+" - "+e.message);
				insole.log("JSON data was: "+response.responseText);
				insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
			}
			if(data.success) {
				newNode.id = "take-"+data.id;
				insole.log("Successfully added take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber);
			} else {
				insole.error("Error adding to database: "+data.error);
				insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". See previous log message.");
			}
		},
		failure: function() {
			insole.alert("Failed to add take to database. Item: "+item.attributes.itemName+", Take Number: "+takeNumber+". Recommend you refresh this page to check data integrity.");
		}
	});
}

/// Reset new take details to defaults
function ILresetTake () {
	$('inpointBox').value = "00:00:00:00";
	$('outpointBox').value = "00:00:00:00";
	$('durationBox').value = "00:00:00:00";
	$('commentBox').value = "";
	$('resultText').innerHTML = "No Good";
	$('resultText').className = "noGood";
}

/// Set the new take's result to "good"
function ILsetGood() {
	$('resultText').innerHTML = "Good";
	$('resultText').className = "good";
}

/// Set the new take's result to "no good"
function ILsetNoGood() {
	$('resultText').innerHTML = "No Good";
	$('resultText').className = "noGood";
}

/// Populate/update the list of items (the tree) when a new node is added
function ILpopulateItemListNew (tree,parentNode,newNode,index) {
	var selectNew = false;
	if(typeof newNode.attributes.itemName != "undefined") selectNew = true;
	ILpopulateItemList(selectNew);
}

/// Populate/update the list of items (the tree) when a new node is added
function ILpopulateItemList(newNode) {
	if (isDefault(newNode)) newNode = false;
	var elSel = $('itemSelector');
	var selectedID = -1;
	if(newNode) {
		selectedID = elSel.options.length -1;
	} else {
		selectedID = elSel.selectedIndex;
	}
	elSel.options.length=0;
	tree.getRootNode().eachChild(function(node) {
		var elOptNew = document.createElement('option');
		elOptNew.text = node.attributes.itemName;
		elOptNew.value = node.id;
		if(elSel.length == selectedID) elOptNew.selected = true;
		try {
			elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
		}
		catch(e) {
			elSel.add(elOptNew); // IE only
		}
	});
}

/// Send changes made to the database
/// @param node the node (item or take) being changed
/// @param dataIndex the piece of information being changed
/// @param value the value it's being changed to
function ILupdateNodeInDB (node,dataIndex,value) {
	var url;
	var name;
	var id;
	var valueToSend = value;
	var ui;
	if(typeof node.attributes.itemName != "undefined") {
		//item
		url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeItem.pl';
		name = node.attributes.itemName;
		id = node.attributes.databaseID;
		if(dataIndex == "itemName") {
			ui = node.getUI().getItemNameEl();
		} else if (dataIndex == "sequence") {
			ui = node.getUI().getSequenceEl();
		}
	} else {
		//take
		url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/changeTake.pl';
		name = "take "+node.attributes.takeNo;
		id = node.id;
		ui = node.getUI().getTakeElement(dataIndex);
		
		if(dataIndex == "inpoint") {
			valueToSend = ILgetFramesTotalFromText(value);
			dataIndex = "start";
			// update out
			var newOut = ILgetTextFromFramesTotal(valueToSend+ILgetFramesTotalFromText(node.attributes.duration));
			node.attributes.out = newOut;
			node.getUI().getTakeElement('out').innerHTML = newOut;
		} else if (dataIndex == "out") {
			var length = ILgetFramesTotalFromText(value)-ILgetFramesTotalFromText(node.attributes.inpoint);
			valueToSend = length;
			dataIndex = "length";
			// update duration
			var newDuration = ILgetTextFromFramesTotal(length);
			node.attributes.duration = newDuration;
			node.getUI().getTakeElement('duration').innerHTML = newDuration;
		} else if (dataIndex == "duration") {
			valueToSend = ILgetFramesTotalFromText(value);
			dataIndex = "start";
			// update out
			var newOut = ILgetTextFromFramesTotal(ILgetFramesTotalFromText(node.attributes.inpoint)+valueToSend);
			node.attributes.out = newOut;
			node.getUI().getTakeElement('out').innerHTML = newOut;
		}
	}

	if(typeof id.substring != "undefined" && (id.substring(0,5) == "take-" || id.substring(0,5) == "item=")) id = id.substring(5);

	// -- Send to database
	Ext.Ajax.request({
		url: url,
		params : {
			dataindex: dataIndex,
			value: valueToSend,
			id: id
		},
		success: function(response) {
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.log("JSON parse error: "+e.name+" - "+e.message);
				insole.log("JSON data was: "+response.responseText);
				insole.alert("Failed to update database for: "+name+". See previous log message.");
				Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
					if(response == "yes") {
						ILloadNewProg();
					}
				});
			}
			if(data.success) {
				if (typeof ui != "undefined" && typeof ui.innerHTML != "undefined") ui.innerHTML = data.value;
				if (typeof node.attributes[dataIndex] != "undefined") node.attributes[dataIndex] = data.value;
				insole.log("Successfully updated database for: "+name);
			} else {
				Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
					if(response == "yes") {
						ILloadNewProg();
					}
				});
				insole.error("Error updating database: "+data.error);
				insole.alert("Failed to update database for: "+name+". See previous log message.");
			}
		},
		failure: function() {
			Ext.MessageBox.confirm('Database Write Error', "Failed to delete item from database: "+node.attributes.itemName+". Would you like to refresh the data to see the current database state?.", function(response){
				if(response == "yes") {
					ILloadNewProg();
				}
			});
			insole.alert("Failed to update database for: "+name+". Recommend you refresh this page to check data integrity.");
		}
	});
}

/// Select the previous item in the item list
function ILselectNextItem() {
	var elSel = $('itemSelector');
	if(typeof elSel.options[elSel.selectedIndex+1] != "undefined") elSel.options[elSel.selectedIndex+1].selected = true;
}

/// Select the next item in the item list
function ILselectPrevItem() {
	var elSel = $('itemSelector');
	if(typeof elSel.options[elSel.selectedIndex-1] != "undefined") elSel.options[elSel.selectedIndex-1].selected = true;
}

/// Populate a list of available recorders from database information
var ILRecorders = new Object();
function ILdiscoverRecorders() {
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/discoverRecorders.pl',
		success: function(response){
			try {
				var nodes = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading recorder list");
			}
			var elSel = $('recorderSelector');
			elSel.options.length=0;
			var elOptNew = document.createElement('option');
			elOptNew.text = "Choose Recorder";
			elOptNew.value = "-1";
			elOptNew.selected = true;
			try {
				elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
			} catch (e) {
				elSel.add(elOptNew); // IE only
			}
			for (node in nodes) {
				if(nodes[node].nodeType != "Recorder") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = node;
				elOptNew.value = node;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			ILRecorders = nodes;
		},
		failure: function()	{
			insole.alert("Error loading recorder list");
		}
	});
}

/// Construct the URL to query a recorder
/// @param recName the recorder's name
/// @return the url
function ILgetRecorderURL (recName) {
	if(typeof ILRecorders[recName] != "undefined") {
		return ILRecorders[recName].ip+queryLoc;
	}
	return false;
}

/// Grab the series and programme lists from the database
var ILseriesData = new Object();
function ILpopulateProgRecInfo() {
	Ext.Ajax.request({
		url: '/cgi-bin/ingex-modules/Logging.ingexmodule/getProgRecInfo.pl',
		success: function(response){
			try {
				var data = JSON.parse(response.responseText);
			} catch (e) {
				insole.alert("Error loading series, programme & recording location lists");
			}
			
			var elSel = $('recLocSelector');
			elSel.options.length=0;
			for (loc in data.recLocs) {
				if(typeof data.recLocs[loc] != "string") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = data.recLocs[loc];
				elOptNew.value = loc;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			
			elSel = $('seriesSelector');
			elSel.options.length=0;
			var elOptNew = document.createElement('option');
			elOptNew.text = "Choose Series";
			elOptNew.value = "-1";
			elOptNew.selected = true;
			try {
				elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
			} catch (e) {
				elSel.add(elOptNew); // IE only
			}
			for (var series in data.series) {
				if(typeof data.series[series] == "function") continue;
				var elOptNew = document.createElement('option');
				elOptNew.text = data.series[series].name;
				elOptNew.value = series;
				try {
					elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
				} catch (e) {
					elSel.add(elOptNew); // IE only
				}
			}
			
			ILseriesData = data.series;
		},
		failure: function()	{
			insole.alert("Error loading series, programme & recording location lists");
		}
	});
}

/// Populate the list of programmes based on the selected series
function ILupdateProgrammes () {
	var seriesID = $('seriesSelector').options[$('seriesSelector').selectedIndex].value;
	
	var programmes = ILseriesData[seriesID].programmes;
	var elSel = $('programmeSelector');
	elSel.options.length=0;
	var elOptNew = document.createElement('option');
	elOptNew.text = "Choose:";
	elOptNew.value = "-1";
	elOptNew.selected = true;
	try {
		elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
	} catch (e) {
		elSel.add(elOptNew); // IE only
	}
	for (var prog in programmes) {
		if(typeof programmes[prog] != "string") continue;
		var elOptNew = document.createElement('option');
		elOptNew.text = programmes[prog];
		elOptNew.value = prog;
		try {
			elSel.add(elOptNew, null); // standards compliant; doesn't work in IE
		} catch (e) {
			elSel.add(elOptNew); // IE only
		}
	}
}

/// Grab a the currently selected programme's item/take info from the database 
function ILloadNewProg () {
	treeLoader.url = '/cgi-bin/ingex-modules/Logging.ingexmodule/data/getProgramme.pl';
	treeLoader.load(tree.getRootNode(),function(){tree.expandAll();});
}

/// Open a printable version of the data
function ILprint () {
	if(typeof $('programmeSelector').options[$('programmeSelector').selectedIndex] == "undefined" || $('programmeSelector').options[$('programmeSelector').selectedIndex].value == -1) {
		Ext.MessageBox.alert('Cannot Print', 'Please select a series and programme before attempting to print.');
		return -1;
	}
	var series = $('seriesSelector').options[$('seriesSelector').selectedIndex].text;
	var programme = $('programmeSelector').options[$('programmeSelector').selectedIndex].text;
	var now = new Date();
	var monthname=new Array("Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec")
	var date = now.getDate()+" "+monthname[now.getMonth()]+" "+now.getFullYear();
	var newwindow=window.open('','ILprintWindow','height=400,width=980,scrollbars=1,resizable=1');
	var tmp = newwindow.document;
	tmp.write('<html><head><title>Ingex Log</title>');
	tmp.write('<link rel="stylesheet" href="/ingex/printpage.css">');
	tmp.write('</head><body><div id="progInfoDiv">');
	tmp.write('<div class="date">Date: '+date+'</div>');
	tmp.write('<div class="producer">Producer: '+$('producer').value+'</div>');
	tmp.write('<div class="director">Director: '+$('director').value+'</div>');
	tmp.write('<div class="pa">PA: '+$('pa').value+'</div></div>');
	tmp.write('<h1>'+series+'</h1>');
	tmp.write('<h2>'+programme+'</h2>');
	tmp.write('<div style="clear:both">&nbsp;</div>');
	tmp.write('<table><tr class="header"><th>TAKE</th><th colspan=2>TIMECODE</th><th>RESULT</th><th>COMMENT</th><th>LOCATION</th><th>DATE</th>');
	var items = tree.getRootNode().childNodes;
	for(var i in items) {
		if(typeof items[i].attributes != "undefined") {
			tmp.write('<tr class="itemHeaderLine"><td colspan=4></td><td class="itemHeader">'+items[i].attributes.itemName+'</td><td colspan=2></td></tr>');
			var takes = items[i].childNodes;
			for(var t in takes) {
				if (typeof takes[t].attributes != "undefined") {
					var a = takes[t].attributes;
					tmp.write('<tr>');
					tmp.write('<td class="take">'+a.takeNo+'</td>');
					tmp.write('<td class="tclabel">in:<br />out:<br />dur:</td>');
					tmp.write('<td class="timecode">'+a.inpoint+'<br />'+a.out+'<br />'+a.duration+'</td>');
					tmp.write('<td class="result">'+a.result+'</td>');
					tmp.write('<td class="comment">'+a.comment+'</td>');
					tmp.write('<td class="location">'+a.location+'</td>');
					tmp.write('<td class="date">'+a.date+'</td>');
					tmp.write('</tr>');
				}
			}
		}
	}
	tmp.write('</table></body></html>');
	tmp.close();
	newwindow.print();
}

/// Import a script (un-implemented)
function ILimport () {
	Ext.MessageBox.alert('Import Unavailable', 'Unfortunately, this feature has not yet been implemented.');
}



// A "class" for a timecode
function ingexLoggingTimecode () {
	this.h = 0;
	this.m = 0;
	this.s = 0;
	this.f = 0;
	this.stopped = false;

	this.durH = 0;
	this.durM = 0;
	this.durS = 0;
	this.durF = 0;

	this.takeRunning = false;

	this.incrementer = false;
	this.getter = false;
	this.ajaxtimeout = false;

	this.startIncrementing = function(getFromServer,reset) {
		if (isDefault(getFromServer)) getFromServer = true;
		if (isDefault(reset)) reset = false;
		if(!this.incrementer || reset) {
			if(getFromServer){
				if($('tcDisplay').innerHTML == "-- : -- : -- : --") {
					showLoading('tcDisplay');
				}
				this.update();
				this.getter = setInterval(function(){ILtc.update();},2000);
			} else {
				this.incrementer = setInterval(function(){ILtc.increment(tcFreq);},(tcFreq*40));
			}
		}
	}

	this.update = function() {
		var xmlHttp = getxmlHttp();
		xmlHttp.onreadystatechange = function () {
			if (xmlHttp.readyState==4) {
				if (ILtc.ajaxtimeout) {
					clearTimeout(ILtc.ajaxtimeout);
					ILtc.ajaxtimeout = false;
					ILtc.startIncrementing(false);
				}
				try {
					var tmp = JSON.parse(xmlHttp.responseText);
					var data = tmp.tc;
					ILtc.set(data.h,data.m,data.s,data.f,data.stopped,false);
				} catch (e) {
					clearInterval(ILtc.incrementer);
					clearInterval(ILtc.getter);
					insole.alert("Received invalid response when updating timecode (JSON parse error). Timecode display STOPPED.");
					insole.log("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
					$('tcDisplay').innerHTML = "<span class='error'>Error</span>";
				}
			}
		};

		var recSel = $('recorderSelector');
		var recorderURL = ILgetRecorderURL(recSel.options[recSel.selectedIndex].value);
		if(!recorderURL) {
			insole.alert("Error querying recorder for timecode - no recorder URL available");
			return false;
		}
		
		var now = new Date();
		var random = Math.floor(Math.random()*101);
		var requestID = now.getHours()+now.getMinutes()+now.getSeconds()+now.getMilliseconds()+random;

		xmlHttp.open("GET","/cgi-bin/ingex-modules/Logging.ingexmodule/tc.pl?url="+recorderURL+"&"+requestID,true);
		xmlHttp.send(null);
		if (this.ajaxtimeout) {
			clearTimeout(this.ajaxtimeout);
		}
		this.ajaxtimeout = setTimeout(function(){ILtc.ajaxTimedout()},5000);
	}
	
	this.ajaxTimedout = function() {
		clearTimeout(this.ajaxtimeout);
		clearInterval(this.incrementer);
		clearInterval(this.getter);
		this.ajaxtimeout = false;
		insole.alert("AJAX timeout occurred when updating timecode. Timecode display STOPPED.");
		$('tcDisplay').innerHTML = "<span class='error'>Error</span>";
	}

	this.set = function(h,m,s,f,stopped,startInc) {
		if (isDefault(startInc)) startInc = true;
		this.h = h;
		this.m = m;
		this.s = s;
		this.f = f;
		this.stopped = stopped;
		if(startInc) this.startIncrementing();
	}
	
	this.increment = function(numToInc) {
		if (isDefault(numToInc)) numToInc = 1;
		var draw = false;
		if (!this.stopped) {
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
			
			// draw
			var fr = "--";
			if (showFrames) fr = f;
			if (showFrames || draw) $('tcDisplay').innerHTML = h+":"+m+":"+s+":"+fr;
			
			if(this.takeRunning) {
				this.durF += numToInc;
				if (this.durF > editrate) {
					this.durF -= editrate;
					this.durS++;
					if (this.durS > 59) {
						this.durS -= 59;
						this.durM++;
						if (this.durM > 59) {
							this.durM -= 59;
							this.durH++;
						}
					}
				}
				// Format as 2 digit numbers
				var dh; var dm; var ds; var df;
				if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
				if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
				if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
				if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
				
				// draw
				var dfr = "--";
				if (showFrames) dfr = df;
				if (showFrames || draw) {
					$('durationBox').value = dh+":"+dm+":"+ds+":"+dfr;
					$('outpointBox').value = h+":"+m+":"+s+":"+fr;
				}
			}
		}
	}
	
	this.startTake = function() {
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		$('inpointBox').value = h + ":" + m + ":" + s + ":" + f;
		$('outpointBox').value = h + ":" + m + ":" + s + ":" + f;
		$('durationBox').value = "00:00:00:00";
		this.durH = 0;
		this.durM = 0;
		this.durS = 0;
		this.durF = 0;
		this.takeRunning = true;
	}
	
	this.stopTake = function() {
		this.takeRunning = false;
		// Format as 2 digit numbers
		var h; var m; var s; var f;
		if (this.h < 10) { h = "0" + this.h; } else { h = this.h; }
		if (this.m < 10) { m = "0" + this.m; } else { m = this.m; }
		if (this.s < 10) { s = "0" + this.s; } else { s = this.s; }
		if (this.f < 10) { f = "0" + this.f; } else { f = this.f; }
		$('outpointBox').value = h + ":" + m + ":" + s + ":" + f;
		
		// Format as 2 digit numbers
		var dh; var dm; var ds; var df;
		if (this.durH < 10) { dh = "0" + this.durH; } else { dh = this.durH; }
		if (this.durM < 10) { dm = "0" + this.durM; } else { dm = this.durM; }
		if (this.durS < 10) { ds = "0" + this.durS; } else { ds = this.durS; }
		if (this.durF < 10) { df = "0" + this.durF; } else { df = this.durF; }
		$('durationBox').value = dh+":"+dm+":"+ds+":"+df;
	}

}

// --- Extending Ext library for Logging Module ---

Ext.tree.ColumnTree = Ext.extend(Ext.tree.TreePanel, {
    lines:false,
    borderWidth: Ext.isBorderBox ? 0 : 2, // the combined left/right border for each cell
    cls:'x-column-tree',

    onRender : function(){
        Ext.tree.ColumnTree.superclass.onRender.apply(this, arguments);
        this.headers = this.body.createChild(
            {cls:'x-tree-headers'},this.innerCt.dom);

        var cols = this.columns, c;
        var totalWidth = 0;

        for(var i = 0, len = cols.length; i < len; i++){
             c = cols[i];
             totalWidth += c.width;
             this.headers.createChild({
                 cls:'x-tree-hd ' + (c.cls?c.cls+'-hd':''),
                 cn: {
                     cls:'x-tree-hd-text',
                     html: c.header
                 },
                 style:'width:'+(c.width-this.borderWidth)+'px;'
             });
        }
        this.headers.createChild({cls:'x-clear'});
        // prevent floats from wrapping when clipped
        this.headers.setWidth(totalWidth);
        this.innerCt.setWidth(totalWidth);
    }
});

Ext.tree.TakeNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    focus: Ext.emptyFn, // prevent odd scrolling behavior

    renderElements : function(n, a, targetNode, bulkRender){
        this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';

        var t = n.getOwnerTree();
        var cols = t.columns;
        var bw = t.borderWidth;
        var c = cols[0];

        var buf = [
             '<li class="x-tree-node"><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                '<div class="x-tree-col" style="width:',c.width-bw,'px;">',
                    '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    //'<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
					'<img src="/ingex/img/take.gif" unselectable="on" class="x-tree-node-icon" style="background-image:none;">',
                    '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    '<span unselectable="on">', n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</span></a>",
                "</div>"];
         for(var i = 1, len = cols.length; i < len; i++){
             c = cols[i];

             buf.push('<div class="x-tree-col ',(c.cls?c.cls:''),'" style="width:',c.width-bw,'px;">',
                        '<div class="x-tree-col-text" dataIndex="',c.dataIndex,'">',(c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),"</div>",
                      "</div>");
         }
         buf.push(
            '<div class="x-clear"></div></div>',
            '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            "</li>");

        if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                n.nextSibling.ui.getEl(), buf.join(""));
        }else{
            this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        }

        this.elNode = this.wrap.childNodes[0];
        this.ctNode = this.wrap.childNodes[1];
        var cs = this.elNode.firstChild.childNodes;
        this.indentNode = cs[0];
        this.ecNode = cs[1];
        this.iconNode = cs[2];
        this.anchor = cs[3];
        this.textNode = cs[3].firstChild;
		this.takeelements = new Object();
		this.takeelements['location'] = this.elNode.childNodes[1].firstChild;
		this.takeelements['date'] = this.elNode.childNodes[2].firstChild;
		this.takeelements['inpoint'] = this.elNode.childNodes[3].firstChild;
		this.takeelements['out'] = this.elNode.childNodes[4].firstChild;
		this.takeelements['duration'] = this.elNode.childNodes[5].firstChild;
		this.takeelements['result'] = this.elNode.childNodes[6].firstChild;
		this.takeelements['comment'] = this.elNode.childNodes[7].firstChild;
    },

	getTakeElement : function(name) {
		if(typeof this.takeelements[name] != "undefined") return this.takeelements[name];
		return false;
	}
});

Ext.tree.ItemNodeUI = Ext.extend(Ext.tree.TreeNodeUI, {
    focus: Ext.emptyFn, // prevent odd scrolling behavior

	getItemNameEl : function(){
		return this.itemNameNode;
	},
	
	getSequenceEl : function(){
		return this.sequenceNode;
	},

    renderElements : function(n, a, targetNode, bulkRender){
        this.indentMarkup = n.parentNode ? n.parentNode.ui.getChildIndent() : '';

        var t = n.getOwnerTree();
        var cols = t.columns;
        var bw = t.borderWidth;
        var c = cols[0];

        var buf = [
             '<li class="x-tree-node"><div ext:tree-node-id="',n.id,'" class="x-tree-node-el x-tree-node-leaf ', a.cls,'">',
                '<div class="x-tree-col" style="width:',c.width-bw,'px;">',
                    '<span class="x-tree-node-indent">',this.indentMarkup,"</span>",
                    '<img src="', this.emptyIcon, '" class="x-tree-ec-icon x-tree-elbow">',
                    // '<img src="', a.icon || this.emptyIcon, '" class="x-tree-node-icon',(a.icon ? " x-tree-node-inline-icon" : ""),(a.iconCls ? " "+a.iconCls : ""),'" unselectable="on">',
					'<img src="/ingex/img/item.gif" unselectable="on">',
                    '<a hidefocus="on" class="x-tree-node-anchor" href="',a.href ? a.href : "#",'" tabIndex="1" ',
                    a.hrefTarget ? ' target="'+a.hrefTarget+'"' : "", '>',
                    '<span unselectable="on">',/* n.text || (c.renderer ? c.renderer(a[c.dataIndex], n, a) : a[c.dataIndex]),*/"</span></a>",
                "</div>"];

            buf.push('<div class="x-tree-col" style="width:500px;border-bottom:1px solid #ddd;color: #449;">',
                        '<div class="x-tree-col-text" dataIndex="itemName">',a['itemName'],"</div>",
                      "</div>");

			var seq = "[no sequence]";
			if (a['sequence'] != "") seq = a['sequence'];
			buf.push('<div class="x-tree-col" style="width:200px;border-bottom:1px solid #ddd;color: #449;">',
                   '<div class="x-tree-col-text" dataIndex="sequence">',seq,"</div>",
                 "</div>");

         buf.push(
            '<div class="x-clear"></div></div>',
            '<ul class="x-tree-node-ct" style="display:none;"></ul>',
            "</li>");

        if(bulkRender !== true && n.nextSibling && n.nextSibling.ui.getEl()){
            this.wrap = Ext.DomHelper.insertHtml("beforeBegin",
                                n.nextSibling.ui.getEl(), buf.join(""));
        }else{
            this.wrap = Ext.DomHelper.insertHtml("beforeEnd", targetNode, buf.join(""));
        }

        this.elNode = this.wrap.childNodes[0];
        this.ctNode = this.wrap.childNodes[1];
        var cs = this.elNode.firstChild.childNodes;
        this.indentNode = cs[0];
        this.ecNode = cs[1];
        this.iconNode = cs[2];
        this.anchor = cs[3];
        this.textNode = cs[3].firstChild;
		this.itemNameNode = this.elNode.childNodes[1].firstChild;
		this.sequenceNode = this.elNode.childNodes[2].firstChild;
    }
});

/**
 * @class Ext.tree.ColumnTreeEditor
 * @extends Ext.Editor
 * Provides editor functionality for inline tree node editing.  Any valid {@link Ext.form.Field} subclass can be used
 * as the editor field.
 * @constructor
 * @param {TreePanel} tree
 * @param {Object} fieldConfig (optional) Either a prebuilt {@link Ext.form.Field} instance or a Field config object
 * that will be applied to the default field instance (defaults to a {@link Ext.form.TextField}).
 * @param {Object} config (optional) A TreeEditor config object
 */
Ext.tree.ColumnTreeEditor = function(tree, fc, config){
    fc = fc || {};
    var field = fc.events ? fc : new Ext.form.TextField(fc);
    Ext.tree.TreeEditor.superclass.constructor.call(this, field, config);

    this.tree = tree;

    if(!tree.rendered){
        tree.on('render', this.initEditor, this);
    }else{
        this.initEditor(tree);
    }
};

Ext.extend(Ext.tree.ColumnTreeEditor, Ext.Editor, {
    /**
     * @cfg {String} alignment
     * The position to align to (see {@link Ext.Element#alignTo} for more details, defaults to "l-l").
     */
    alignment: "l-l",
    // inherit
    autoSize: false,
    /**
     * @cfg {Boolean} hideEl
     * True to hide the bound element while the editor is displayed (defaults to false)
     */
    hideEl : false,
    /**
     * @cfg {String} cls
     * CSS class to apply to the editor (defaults to "x-small-editor x-tree-editor")
     */
    cls: "x-small-editor x-tree-editor",
    /**
     * @cfg {Boolean} shim
     * True to shim the editor if selects/iframes could be displayed beneath it (defaults to false)
     */
    shim:false,
    // inherit
    shadow:"frame",
    /**
     * @cfg {Number} maxWidth
     * The maximum width in pixels of the editor field (defaults to 250).  Note that if the maxWidth would exceed
     * the containing tree element's size, it will be automatically limited for you to the container width, taking
     * scroll and client offsets into account prior to each edit.
     */
    maxWidth: 250,
    /**
     * @cfg {Number} editDelay The number of milliseconds between clicks to register a double-click that will trigger
     * editing on the current node (defaults to 350).  If two clicks occur on the same node within this time span,
     * the editor for the node will display, otherwise it will be processed as a regular click.
     */
    editDelay : 350,

    initEditor : function(tree){
        tree.on('beforeclick', this.beforeNodeClick, this);
        tree.on('dblclick', this.onNodeDblClick, this);
        this.on('complete', this.updateNode, this);
        this.on('beforestartedit', this.fitToTree, this);
        //this.on('startedit', this.bindScroll, this, {delay:10});
        this.on('specialkey', this.onSpecialKey, this);
    },

    // private
    fitToTree : function(ed, el){
        var td = this.tree.getTreeEl().dom, nd = el.dom;
        if(td.scrollLeft >  nd.offsetLeft){ // ensure the node left point is visible
            td.scrollLeft = nd.offsetLeft;
        }
        var w = Math.min(
                this.maxWidth,
                (td.clientWidth > 20 ? td.clientWidth : td.offsetWidth) - Math.max(0, nd.offsetLeft-td.scrollLeft) - /*cushion*/5);
        this.setSize(w, '');
    },

    // private
    triggerEdit : function(node, defer, e){
        this.completeEdit();
		this.targetElement = e.getTarget();
		if(node.attributes.editable !== false && Ext.get(this.targetElement).hasClass('x-tree-col-text')){
	       /**
	        * The tree node this editor is bound to. Read-only.
	        * @type Ext.tree.TreeNode
	        * @property editNode
	        */
			this.editNode = node;
            if(this.tree.autoScroll){
                //node.ui.getEl().scrollIntoView(this.tree.body);
            }
            this.autoEditTimer = this.startEdit.defer(this.editDelay, this, [this.targetElement/*node.ui.textNode, node.text*/]);
            return false;
        }
    },

    // private
    bindScroll : function(){
        this.tree.getTreeEl().on('scroll', this.cancelEdit, this);
    },

    // private
    beforeNodeClick : function(node, e){
        clearTimeout(this.autoEditTimer);
        if(this.tree.getSelectionModel().isSelected(node)){
            e.stopEvent();
            return this.triggerEdit(node, null, e);
        }
    },

    onNodeDblClick : function(node, e){
        clearTimeout(this.autoEditTimer);
    },

    // private
	updateNode : function(ed, value){
		if(value == "") value = "[no data]";
		var dataIndex = this.targetElement.getAttribute('dataIndex')
		this.tree.getTreeEl().un('scroll', this.cancelEdit, this);
		var oldValue = this.editNode.attributes[dataIndex];
		//this.editNode.text = value;
		this.editNode.attributes[dataIndex] = value;
		if(this.editNode.rendered){ // event without subscribing
			this.targetElement.innerHTML = value;
		}
		this.editNode.fireEvent("valuechange", this.editNode, dataIndex, value, oldValue);
	},

    // private
    onHide : function(){
        Ext.tree.TreeEditor.superclass.onHide.call(this);
        if(this.editNode){
            this.editNode.ui.focus.defer(50, this.editNode.ui);
        }
    },

    // private
    onSpecialKey : function(field, e){
        var k = e.getKey();
        if(k == e.ESC){
            e.stopEvent();
            this.cancelEdit();
        }else if(k == e.ENTER && !e.hasModifier()){
            e.stopEvent();
            this.completeEdit();
        }
    }
});