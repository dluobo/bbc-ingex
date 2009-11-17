// Copyright (C) 2009  British Broadcasting Corporation
// Author: Sean Casey <seantc@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.


/*****************************************************************************************
 * Materials javascript
 * Main javascript class for materials page
 *****************************************************************************************/
 
// Show debug alerts?
var debug = false;

// ajax timeout period
var timeout_period = 240000;	// 4 mins	

var dbRootNode = '';
var rootNode;
var userSettings;
var leafNodes = [];
var url;
var tree;
var dropBox;
var panel;
var fromDate;
var toDate;
var filterStyle;
var initialised = false;
var options = [];
var noSelectedItems = 0; 	// number of selected materials items
var destPkgSize = 0;		// size of package on destination tree
var selectedSrcNodes = ''; 	// currently selected nodes on source tree
var selectedDestNodes = '';	// currently selected nodes on destination tree
var toDelete = ''; 			// nodes flagged for deltion
var isLoading = false;
var dndMode = true;
var multiResSelected = false;
var currentFormat = 0;			// current video format
var selectionFormat = 0;			// format of currently selected packages

var onLoad_Material = function() {
	checkJSLoaded(function(){
		init();
	});	
}

// initialise
loaderFunctions.Material = onLoad_Material;


// ext override, necessary for ext multiselect tree
Ext.override(Ext.tree.TreeDropZone, { 
	 completeDrop : function(de) { 
	 	var ns = de.dropNode, p = de.point, t = de.target; 
	 	if(!Ext.isArray(ns)) { 
	 		ns = [ns]; 
	 	} 
	 	var n, node, ins = false; 
	 	if (p != 'append') { 
	 		ins = true; node = (p == 'above') ? t : t.nextSibling; 
	 	} 
	 	for(var i = 0, len = ns.length; i < len; i++) { 
			n = ns[i];
		 			 
			if (ins) { 
				t.parentNode.insertBefore(n, node); 
			} 
			else { t.appendChild(n); 
			} 
			if(Ext.enableFx && this.tree.hlDrop) { 
				n.ui.highlight(); 
			} 
	 	}
		ns[0].ui.focus(); 
		t.ui.endDrop(); 
		this.tree.fireEvent("nodedrop", de);
	} 
}); 

// ext override, to modify timeout value
Ext.tree.TreeLoader.override({
    requestData : function(node, callback){
        if(this.fireEvent("beforeload", this, node, callback) !== false){
            this.transId = Ext.Ajax.request({
                method:this.requestMethod,
                url: this.dataUrl||this.url,
                success: this.handleResponse,
                failure: this.handleFailure,
                timeout: this.timeout || 300000,	// 5 mins
                scope: this,
                argument: {callback: callback, node: node},
                params: this.getParams(node)
            });
        }else{
            // if the load is cancelled, make sure we notify
            // the node that we are done
            if(typeof callback == "function"){
                callback();
            }
        }
    }
}); 
	
// set ext spacer image to a local file instead of remote url
Ext.BLANK_IMAGE_URL = "/ingex/ext/resources/images/default/tree/s.gif";


/*
 * initialise
 */
function init() {
	//have ext components already been loaded?
	var renderedExt = false;
	if($('tree').innerHTML){renderedExt = true;}
	
	//test for drag and drop mode
	// if 'aafbasket' exist, the page is configured for drag/drop mode
	if(document.getElementById('aafbasket')){
		dndMode = true;
	}
	else{
		dndMode = false;
	}
	
	// user settings cookie object
	userSettings = new UserSettings(document);
	
	// select default format
	$('v_res').options[8].selected = true;

	if(!renderedExt){	// ensure only initialised once
		// from and to date pickers
		var today = new Date();
		
		fromDate = new Ext.form.DateField({
			renderTo: 'from_cal',
			name: 'date',
			width: '150px',
			value: today,
			format: 'd/m/y'
		});
		toDate = new Ext.form.DateField({
			renderTo: 'to_cal',
			name: 'date',
			width: '100px',
			value: today,
			format: 'd/m/y'
		});
		
		fromDate.disabled = 'true';
		toDate.disabled = 'true';
		
		// tooltips
		if(dndMode){
			new Ext.ToolTip({
				target:	'tree',
				html:	'Browse and select original material items from here'
			});
			new Ext.ToolTip({
				target:	'aafbasket',
				html:	'Drag any materials you want to export into here'
			});
		}
		else{
			new Ext.ToolTip({
				target:	'tree',
				html:	'Matching material items'
			});
		}
		
		// root nodes	 
		rootNode = new Ext.tree.AsyncTreeNode({
			allowChildren: true,
			text: 'Projects',
			id: '0',		//level depth
			name: 'All Projects'
		});
		if(dndMode){
			dbRootNode = new Ext.tree.TreeNode({
				allowChildren: true,
				text: 'Package',
				id: 'dbRoot',
				name: 'Package',
				children: []
			});
		}
		
		// tree node dynamic loaders
		var tLoader = new Ext.tree.TreeLoader({
	        dataUrl: '../cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl',
	 		uiProviders:{
				'col': Ext.tree.ColumnNodeUI
			},
			listeners: {
				loadexception: treeLoadException
			}
		});
		var dbLoader = new Ext.tree.TreeLoader({
			dataUrl: '../cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl',
			uiProviders:{
				'col': Ext.tree.ColumnNodeUI
			},
			listeners: {
				loadexception: treeLoadException
			}
		});
		// source materials tree
		tree = new Ext.tree.MultiSelColumnTree({
			width: 800,
			height: 300,
			rootVisible: true,
			useArrows:true,
			autoScroll: true,
			animate:true,
			renderTo: 'tree',
			enableDrag: true, /* required as of MultiSelectTree v1.1 */
			containerScroll: true,
			ddGroup: 'tree',
			loader: tLoader,
			root: rootNode,
			columns: [
			 	{header: "Name", width: 310, sortable: true, dataIndex: 'name'},
		    	{header: "Created", width: 115, sortable: true, dataIndex: 'created'},
		    	{header: "Start", width: 70, sortable: true, dataIndex: 'start'},
		    	{header: "End", width: 70, sortable: true, dataIndex: 'end'},
		      	{header: "Duration", width: 55, sortable: true, dataIndex: 'duration'},
		     	{header: "Format", width: 95, sortable: true, dataIndex: 'video'},
		     	{header: "TapeID", width: 65, sortable: true, dataIndex: 'tapeid'}
			],
			autoExpandColumn: 'name',
			listeners: {
				click: materialClickedSrc
			}
		});
		
		// if drag/drop enabled
		if(dndMode){
			// destination materials tree
			dropBox = new Ext.tree.MultiSelColumnTree({
				width: 800,
				height: 300,
				rootVisible: true,
				useArrows:true,
				autoScroll: true,
				animate:true,
				renderTo: 'aafbasket',
				enableDrag: true, /* required as of MultiSelectTree v1.1 */
				ddGroup: 'tree',
				loader: dbLoader,
				root: dbRootNode,
				columns: [
				 	{header: "Name", width: 310, sortable: true, dataIndex: 'name'},
		       		{header: "Created", width: 115, sortable: true, dataIndex: 'created'},
		       		{header: "Start", width: 70, sortable: true, dataIndex: 'start'},
		       		{header: "End", width: 70, sortable: true, dataIndex: 'end'},
		       		{header: "Duration", width: 55, sortable: true, dataIndex: 'duration'},
		       		{header: "Format", width: 95, sortable: true, dataIndex: 'video'},
		       		{header: "TapeID", width: 65, sortable: true, dataIndex: 'tapeid'}
				],
				autoExpandColumn: 'name',
				listeners: {
					append: function(){
					},
					click: materialClickedDest
				}
			});
			
			// drag/drop targets
			var panelDropTargetEl = dropBox.body.dom;
			
			var panelDropTarget = new Ext.dd.DropTarget(panelDropTargetEl, {
				ddGroup	: 'tree',
				notifyEnter: 	function(ddsource, e, data){
									// add some style
									dropBox.body.stopFx();
									dropBox.body.highlight();
								},
				notifyDrop: function(ddSource, e, data){
					if(debug){alert('dd');}	
						// copy selected nodes 
						copyNodesAndPaths(data.nodes);
					}
				}
			);
		}
	}
		
	destPkgSize = 0;
    initialised = true;
    
    //refresh
    submitFilter();
	
	//get default export options
	
	//get settings from local cookie if stored
	var settings = loadDefaultExportSettings();
	
	if(settings['FCP']){
		settings['fromcookie'] = 'TRUE';
		setExportDefaults(settings);
		updateEnabledOpts();
	}
	else{
		//no cookie stored - load global defaults from config file on web server instead
		loadExportOptions();
	}
}


/*
 * material item clicked on source tree
 */
function materialClickedSrc(node, e){materialClicked(node, e, 'src');}


/*
 * material item clicked on destination tree
 */
function materialClickedDest(node, e){materialClicked(node, e, 'dest');}


/*
 * called when a material item is clicked
 */
function materialClicked(node, e, elId){
	var singleNode = false;
	
	//store node
	if(elId == 'src'){
		selectedSrcNodes = removeDuplicateSiblings(tree.getSelectionModel().getSelectedNodes());
		if(selectedSrcNodes.length == 1){
			singleNode = true;
		}
	}
	else if(elId == 'dest'){
		selectedDestNodes = removeDuplicateSiblings(dropBox.getSelectionModel().getSelectedNodes());
		singleNode = true;
		if(selectedDestNodes.length == 1){
			singleNode = true;
		}
	}
	
	if(singleNode && node.attributes.leaf){
		noSelectedItems = 1;	//only 1 node selected
		
		//check is materials node
	
		var el = Ext.getDom('m_text');
		url = node.attributes['url'];
		
		
		var trackLinks = '';
		var noTracks = 0;
		
		urls = node.attributes['url'];
		for (var key in urls){
			var trackName = key;
			var url = urls[key];
			trackLinks = trackLinks + createTrackLink(trackName, url);
			noTracks++;
		}
		
		
		if(elId == 'src'){
			document.getElementById('package_src').style.visibility = 'visible';
			document.getElementById('tracks_src').innerHTML = trackLinks;
			
			var comments = node.attributes['comments'];
			document.getElementById('meta_comments_src').innerHTML = comments;
			
			var description = node.attributes['description'];
			document.getElementById('meta_description_src').innerHTML = description;
		
			
			//show size of selected package
 			document.getElementById('status_bar_src').innerHTML = '"' + node.attributes['name'] + '" selected, containing ' + noTracks + ' tracks';
	
		}
		else{
			
			document.getElementById('package_dest').style.visibility = 'visible';
			document.getElementById('tracks_dest').innerHTML = trackLinks;
			
			var comments = node.attributes['comments'];
			document.getElementById('meta_comments_dest').innerHTML = comments;
			
			var description = node.attributes['description'];
			document.getElementById('meta_description_dest').innerHTML = description;
		
		}
		
	}
	else{
		
		//show node size if on source tree
		if(elId == 'src'){
			var size = 0;
			
			for(var i in selectedSrcNodes){
				if(selectedSrcNodes[i].attributes){
					size += countPath(selectedSrcNodes[i], elId);
				}
			}
			
			//only show size of selected nodes on source tree 
			setSrcPkgSize(size);
			document.getElementById('package_src').style.visibility = 'hidden';
		}
		
		//show nothing if on destination tree
		else if(elId == 'dest'){
			document.getElementById('package_dest').style.visibility = 'hidden';
		}
	}
}
	

/*
 * returns the number of material items on this path
 */
function countPath(node, elId){
	var pathSize;
	if(node.attributes.leaf){pathSize = 1;}
	else{pathSize = node.attributes.materialCount;}
	return pathSize;
}


/*
 * get the top level parent of a node (1 level below root) 
 */
function getParent(n){
	var prev = '';
	
	var next = n;
	
	for(var i=0; true; i++){
		
		if(next == rootNode){
			//stop
			return prev;
		}
		
		var copy = new Ext.tree.TreeNode(
			Ext.apply({}, next.attributes)
		);
		if(i>0){copy.appendChild(prev);}
		prev = copy;
		
		var next = next.parentNode;
	}	
}


/*
 * create a copy of a node and it's children
 */
function nodeCopy(n){
	
	var child = n.firstChild;
	
	if(!n.attributes['leaf'] && !child){
		//there are unloaded children, so create a self-loading AsyncTreeNode
		var copy = new Ext.tree.AsyncTreeNode(
			Ext.apply({}, n.attributes)
		);
		copy.attributes['full'] = true;	//indicates that this path contains all the original materials
	}
	else{
		var copy = new Ext.tree.TreeNode(
			Ext.apply({}, n.attributes)
		);
	}
	
	
	if(child != null){
		var childCopy = nodeCopy(child);	//recursive copy
		copy.appendChild(childCopy);	
	}
	
	return copy;	//return the copy
}


/*
 * generate a graphical html link to a track url
 */
function createTrackLink(trackName, url){
	//add soft hyphens to url, to ensure it can be split
	var splitUrl = '';
	var next;
	var out;
	var av = trackName.substring(0,1); //audio or video
	
	out = '<a href=/cgi-bin/ingex-modules/Material.ingexmodule/download.pl?fileIn=' + url + '><div class=track_link_' + av + '>' + trackName + '</div></a>';
	return out;
}


/*
 * takes in node array and removes any nodes that are nth generation children of any other nodes in the array
 * to avoid repetition
 */
function removeDuplicateSiblings(nodes){
	var ret = [];
	ret[0] = nodes[0];
	var j=0;
	
	for(var i=1; i<nodes.length; i++){
		var next = nodes[i];
		var prev = ret[j];
		
		//only keep nodes that are not children of previously encountered ones
		if(!next.isAncestor(prev)){
			j++;
			ret[j] = next;
		}
	}
	
	return ret;
}


/*
 * add path to total count of destination nodes
 * takes array of nodes as argument
 */
function addTotalCount(nodes){
	for(var i=0; i<nodes.length; i++)
		{
		if (!nodes[i]){return;}
		
		var node = nodes[i];
		
		//this is a leaf
		if(node.attributes.leaf){
			//add item to total count
			destPkgSize += 1;
			
			//update 
 			document.getElementById('status_bar_dest').innerHTML = "Output package contains " + destPkgSize + " material items<br>";
		}
		
		//this is a folder
		else{
			countPath(node, 'dest');
		}
	}
}


/*
 * see where node 'source' should fit into tree 'dest'
 * 
 * algorithm knows that source node should be 'full'
 * it looks for a point in the destination where the node should sit
 * 
 * INPUT: 
 * A source root tree node
 * A destination root tree node
 * 
 * OUTPUT:
 * A source node containing changes that needs to be inserted
 * A destination point at which the node fits
 */
function getNodeInsertPoint(source, dest){
	if(!dest.hasChildNodes){return [source, dest];} //no child nodes exist 
	var children = dest.childNodes;
	
	if(debug){alert("looking for " + source.attributes.name + " in " + dest.attributes.name);}
	
	for(var i=0; i<children.length; i++){
		var child = children[i];
		if (!child.attributes){break;}	//ensures no null values are copied
		
		if(child.attributes.name == source.attributes.name){
			//found it!
			if(child.attributes['full']){
				//already full - do not copy!
				return [-1,-1];
			}
			else{
				//check child node
				if(source.hasChildNodes && source.firstChild){
					var val = getNodeInsertPoint(source.firstChild, child);
					if(val){return val;}
				}
			}
		}
	}
	
	//no matched folder names here...
	//copy to this path
	return [source, dest];
}


/*
 * check that multiple video resolutions are not contained in the dragged nodes and that
 * the resolution matches the packages currently in the selection
 */
function areNodesValid(nodes){
	// check they are all same res
	if(multiResSelected){
		return false;
	}
	
	//check this res matches any currently selected nodes
	if(destPkgSize > 0 && currentFormat != selectionFormat){
		return false;
	}
	
	return true;
}


/*
 * takes array of selected nodes and copies them to the destination tree
 * also updates node count for destination tree
 */
function copyNodesAndPaths(nodes){
	
	if(!areNodesValid(nodes)){
		show_static_messagebox("Invalid Selection", "The package selection should contain only one video format. Select the desired format from the search options.");
		return;
	}
	
	for(var i=0; nodes.length; i++){
		if(debug){alert('next node');}
		
		if (!nodes[i]){break;}
		if (!nodes[i].attributes){break;}	//ensures no null values are copied
		
		//if root node has been selected, copy all it's child nodes (the project names)
		if(nodes[i] == rootNode){
			nodes = rootNode.childNodes;
		}
		
		var n = nodes[i];
		
		//this is a leaf - do not allow adding of single leaves
		if(n.attributes.leaf){
			show_static_messagebox("Invalid Selection", "Individual packages cannot be selected. Select the whole multicamera group.");
			return;
		}
	
		//keep the path
		var node = getParent(n);
		
		if(debug){alert("top level parent: " + node.attributes['name'] );}
		
		var sourceNode = node;	//start at project name
		var destNode = dbRootNode;			//start at root 
		
		var nodeName = sourceNode.attributes['name'];
		
		//now see where the path fits within the destination tree
		var ret = getNodeInsertPoint(sourceNode, destNode);
		sourceNode = ret[0];
		nodePos = ret[1];
		if(!nodePos){nodePos = destNode;}
		if(debug){alert("insert point for node: " + nodePos);}
	
		//matched source path with destination - do not copy -
		if(nodePos == -1){
			
		}
		
		//no match found for nodeName, so insert node as child at this level
		else{
			var copy = nodeCopy(sourceNode); 
			var nodeName = copy.attributes['name'];
			
			//if this node already exists at the destination, remove it so it can be replaced with the new one
			if(nodePos.findChild('name', nodeName)){
				//get child
				var child = nodePos.findChild('name', nodeName);
				
				if(debug){alert(child.attributes['name']);}
				//deduct the nodes it contains
				var materialCount = getMaterialCount(child);
				destPkgSize -= materialCount;
				if(debug){alert("removing " + materialCount + " items");}
				
				//remove it
				nodePos.removeChild(child);
				
				//update 
 				document.getElementById('status_bar_dest').innerHTML = "Output package contains " + destPkgSize + " material items<br>";
 				
 				if(debug){alert("new dest pkg size: " + destPkgSize);}
			}
			
			//add the new node
			nodePos.appendChild(copy);


			//now add nodes to the total node count
			
			//this is a folder
			
			//add folder size to total count
			destPkgSize += countPath(n, 'dest');
		}
	}
	
	//update destination package count
	setDestPkgSize(destPkgSize);
	
	//set root node to count
	dbRootNode.attributes['materialCount'] = destPkgSize;
	
	//the current video format
	selectionFormat = currentFormat;
}


/*
 * create set of parameters that describe the supplied node path
 */
function getPathParams(node)
{
	//other options are already populated
	var id = node.attributes['id'];
	options['project'] = '';
	options['date'] = '';
	options['time'] = '';
	
	if(id.match('^1')){
		//project name
		options['project'] = node.attributes['projId'];
	}
	else if(id.match('^2')){
		//project name
		options['project'] = node.parentNode.attributes['projId'];
		//date
		options['date'] = node.attributes['name'];
	}
	else if(id.match('^3')){
		//project name
		options['project'] = node.parentNode.parentNode.attributes['projId'];
		//date
		options['date'] = node.parentNode.attributes['name'];
		//time
		options['time'] = node.attributes['name'];
	}
	else{
		//some other folder selected
		noSelectedItems = 0;
	}
	
	return options;
}


/*
 * create an avid or final cut package using materials in destination tree
 */
function packageAAF(){
	if(multiResSelected){
		//do not allow user to export as > 1 resolution in selection
		show_static_messagebox("Invalid Selection", "The package selection contains more than one video format. Select the desired format from the search options.");
		return;
	}
	
	show_wait_messagebox("Packaging AAF", "Please wait...");
	leafNodes = [];
	
	if(dndMode){getLeaves(dbRootNode);}	//drag/drop - load only nodes that have been dragged into selction tree
	else{getLeaves(rootNode);}	//not drag/drop - get all loaded nodes

	//got all leaves - execute create_aaf
	loadComplete();	
}


/*
 * get all leaf nodes in destination tree
 */
function getLeaves(node){
	if(debug){alert("getting leaves of " + node.attributes['name']);}
	
	if(dndMode){
		if(node instanceof Ext.tree.AsyncTreeNode && node.getDepth() > 0){	//check not root node
			if(!node.attributes['leaf'] && !node.isLoaded()){
                insole.log('unloaded folder');
				//there are unloaded children, so this path will contain all original materials
				var materialIdsStr = node.attributes['materialIds'];
				var materialIds = materialIdsStr.split(",");
				
				for(var i=0; i<materialIds.length; i++)
				{
					leafNodes.push(materialIds[i]);
				}
			}
            else if(!node.attributes['leaf'] && node.isLoaded()){
			    //this is a loaded folder, which contians some children

			    //investigate each child
			    var children = node.childNodes;

			    for(var i=0; i<children.length; i++)
                {                 
				    if(children[i].attributes){
					    getLeaves(children[i]);
                    }
                }

                return;
            }
		}
		else if(node.attributes['leaf'] && node.attributes.id>0){
			//this is a material leaf
			leafNodes.push(node.attributes.id);
			return;
		}
		else{
			//this is a loaded folder, which contians some children
		
			//investigate each child
			var children = node.childNodes;
			
			for(var i=0; i<children.length; i++)
			{
				if(children[i].attributes){
					getLeaves(children[i]);
				}
			}
			
			return;
		}
	}
	
	//non-dnd mode
	else{
		if(node.getDepth() == 0){	//is root node
			//investigate each project node
			var children = node.childNodes;
			
			for(var i=0; i<children.length; i++)
			{
				if(children[i].attributes){
					getLeaves(children[i]);
				}
			}
			
			return;
		}
		else{	//get ids from each 'project' node
			var materialIdsStr = node.attributes['materialIds'];
			var materialIds = materialIdsStr.split(",");
				//alert("getting " + materialIdsStr);	
			for(var i=0; i<materialIds.length; i++)
			{
				leafNodes.push(materialIds[i]);
			}
		}
	}
}

  
/*
 * loaded all leaves from tree - continue
 */
function loadComplete(){
	//check if user wants to save export options as defaults
	var saveOpts = document.getElementById('save').checked;	//save as defaults?
	
	if(saveOpts == true){
		var response = confirm("These export settings will be used as defaults on this computer. Continue?");
		
		if(response){
			//yes - continue
		}
		else{
			//do nothing
			return;
		}
	}
	
	if(debug){alert("load complete!");}
	var clipPkg = [];
	var j=0;
	
	
	// create JSON settings for create_aaf script
	
	// get leaf node parameters
	for(var i in leafNodes){
		if(leafNodes[i]>0){clipPkg[j] = {"PkgID":leafNodes[i]}; j++;}
	}

	// get export parameters
	var format = document.getElementById('format').value ;
	var fcp;
	if (format == 'fcp') {fcp = "TRUE";}
		else {fcp = "FALSE";}
	var dircut = $('dircut').checked.toString().toUpperCase();
	var dircutaudio = $('dircutaudio').checked.toString().toUpperCase();
	var exportdir = $('exportdir').value;
	var fnameprefix = $('fnameprefix').value;
	var longsuffix = $('longsuffix').checked.toString().toUpperCase();
	var editpath = $('editpath').value;
//	var dirsource = $('dirsource').value;
	var dirsource = ''; 	
	
	var json = 	{"Root": 
					[{
						"ApplicationSettings": 
							[{"FnamePrefix": fnameprefix, "ExportDir": exportdir, "FCP": fcp, "LongSuffix": longsuffix, "EditPath": editpath, "DirCut": dircut, "DirSource": dirsource, "AudioEdit": dircutaudio}],
						"ClipSettings": clipPkg,
						"RunSettings":
							[{"GroupOnly": "TRUE", "Group": "TRUE", "MultiCam": "TRUE", "NTSC":"FALSE", "Verbose":"FALSE", "DNS":"", "User":"", "Password":""}]
					}]
				};
					
	var jsonText = JSON.stringify(json);
	
	if(saveOpts);
	saveDefaultExportSettings(json.Root[0].ApplicationSettings[0]);
	createAAF(jsonText);	// call script to create the package
}


/*
 * save export settings locally in web cookie
 */
function saveDefaultExportSettings(settings){
	var val;
	
	for(var key in settings){
		val = settings[key];
		userSettings.set(key, val);
	}
	
	userSettings.save();
}


/*
 * load export settings locally from web cookie
 */
function loadDefaultExportSettings(){
	//user settings cookie object
	var params = userSettings.getAll();
	return params;
}


/*****************************************************************************************
 * enable/disable relevant html form elements
 */
 
// aaf export options
function updateEnabledOpts(){
	var format = $('format').value;
	var fcp;
	if (format == 'fcp') 
	{
		$('editpath').disabled = false;  //final cut pro selected
	}
	else 
	{
		$('editpath').disabled = true;
	}
	
	var dircut = $('dircut').checked;
	if (dircut)
	{
//		$('dirsource').disabled = false;
		$('dircutaudio').disabled = false;
	} 
	else
	{
//		$('dirsource').disabled = true;
		$('dircutaudio').disabled = true;
	}
}

// all times
function allTimesSet(){
	filterStyle = 'all';
	
	fromDate.disabled = true;
	toDate.disabled = true;
	
	$('hh_from').disabled = true;
	$('mm_from').disabled = true;
	$('ss_from').disabled = true;
	$('ff_from').disabled = true;
	
	$('hh_to').disabled = true;
	$('mm_to').disabled = true;
	$('ss_to').disabled = true;
	$('ff_to').disabled = true;
	
	$('range_in').disabled = true;
	$('day_in').disabled = true;
}

// enable day/period fields
function dayFilterSet(){
	filterStyle = 'day';
	
	fromDate.disabled = true;
	toDate.disabled = true;
	
	$('hh_from').disabled = true;
	$('mm_from').disabled = true;
	$('ss_from').disabled = true;
	$('ff_from').disabled = true;
	
	$('hh_to').disabled = true;
	$('mm_to').disabled = true;
	$('ss_to').disabled = true;
	$('ff_to').disabled = true;
	
	$('range_in').disabled = false;
	$('day_in').disabled = false;
}


// enable start/end date and time fields
function dateFilterSet(){
	filterStyle = 'date';
	
	fromDate.disabled = false;
	toDate.disabled = false;
	
	$('hh_from').disabled = false;
	$('mm_from').disabled = false;
	$('ss_from').disabled = false;
	$('ff_from').disabled = false;
	
	$('hh_to').disabled = false;
	$('mm_to').disabled = false;
	$('ss_to').disabled = false;
	$('ff_to').disabled = false;
	
	$('range_in').disabled = true;
	$('day_in').disabled = true;
}
/*
 * end: enable/disable relevant html form elements
 *****************************************************************************************/

 
 /*
  * apply a filter to the source nodes
  */
 function submitFilter(){
 	$('filter_matches').innerHTML = ''; //clear number of matching material items text
 	$('status_bar_src').innerHTML = '';	//clear currently selected node text 
 	
 	if(!formValid()){
 		//show an error
 		input_err('time_range_err');
 		return;
 	}

 	//form is valid
 	clear_input_errs();
 	
 	//collect form elements
 	options['projname'] = $('proj_name').value;
 	options['format'] = $('v_res').value;
 	
 	currentFormat = options['format']; 
 	currentFormat == -1 ? multiResSelected = true :	multiResSelected = false;
 	
 	if($('all_time_radio').checked){
 		//select all times
 		options['fromtime'] = 0;
 		options['totime'] = 0; 
 		
 		options['range'] = 0; 
 		options['day'] = 0; 
 	}
 	
 	else if($('time_period_radio').checked){
 		//select times in the time period->day
 		options['fromtime'] = 0;
 		options['totime'] = 0; 
 		
 		options['range'] = $('range_in').value;
 		options['day'] = $('day_in').value;
 	}
 	
 	else if($('time_range_radio').checked){
 		//select times from startdate->enddate
 		options['fromtime'] = getFromTime();	//unix timestamp 
 		options['totime'] = getToTime();
 		
 		options['range'] = 0; 
 		options['day'] = 0; 
 	
 	}
 	
 	options['searchtext'] = $('search_text').value;	//space separated keywords
 	
 	countPackages(options);
 	refreshTree(options);
 }
 
 
/*
 * Get timestamp for 'time from' period
 */
function getFromTime(){
	var time = fromDate.getValue().getTime();
 	
 	var hh = parseInt($('hh_from').value, 10);
 	var mm = parseInt($('mm_from').value, 10);
 	var ss = parseInt($('ss_from').value, 10);
 	var ff = parseInt($('ff_from').value, 10);	//frame no 0-24
 	
 	time += ((ff / 25) * 1000)
 		 +  (ss * 1000)
 		 +  (mm * 60 * 1000)
 		 +  (hh * 60 * 60 * 1000);
 
	return time;
}
 

/*
 * Get timestamp for 'time to' period
 */
function getToTime(){
	var time = toDate.getValue().getTime();
	
 	var hh = parseInt($('hh_to').value, 10);
 	var mm = parseInt($('mm_to').value, 10);
 	var ss = parseInt($('ss_to').value, 10);
 	var ff = parseInt($('ff_to').value, 10);
 	
	time += ((ff / 25) * 1000)
 		 +  (ss * 1000)
 		 +  (mm * 60 * 1000)
 		 +  (hh * 60 * 60 * 1000);
 
	return time;
}
 

 /*
  * display output materials size
  */
 function setDestPkgSize(size){
 	if(debug){alert('setdestpkgsize');}
 	//update 
 	$('status_bar_dest').innerHTML = "Output package contains " + size + " material items<br>";
 }
 
 
  /*
   * display selected source materials size
   */
 function setSrcPkgSize(size){
 	if(debug){alert('setsrcpkgsize');}
 		//show size of selected folder
	 	if(selectedSrcNodes.length == 1){	//only one node selected
	 		$('status_bar_src').innerHTML = selectedSrcNodes[0].attributes.name + ' selected, containing ' + size + ' material items';
	 	}
	 	else{	//multiple folders
	 		$('status_bar_src').innerHTML = 'Multiple items selected, containing ' + size + ' material items';
	 	}
 }
 

 
 /*
  * called to notify user of an error in form input
  */
 function input_err(item){
 	$(item).style.visibility = 'visible';
 }
 
 
 /*
  * hide all input errors
  */
 function clear_input_errs(){
 	$('time_range_err').style.visibility = 'hidden';
 }
 
 
 /*
  * check validity of form elements
  */
 function formValid(){

	//check validity of time/date range fields
 	if($('time_range_radio').checked){
 		//date validity is ensured by ext api
 		
 		//time validity
 		var hh = parseInt($('hh_from').value) 	//is an int?
 		if(hh > 23 || hh < 0 || isNaN(hh)){return false;}
 
 		var mm = parseInt($('mm_from').value) 	//is an int?
 		if(mm > 59 || mm < 0 || isNaN(mm)){return false;}
 		
 		var ss = parseInt($('ss_from').value) 	//is an int?
 		if(ss > 59 || ss < 0 || isNaN(ss)){return false;}
 		
 		var ff = parseInt($('ff_from').value) 	//is an int?
 		if(ff > 24 || ff < 0 || isNaN(ff)){return false;}
 		
 		var hh = parseInt($('hh_to').value) 	//is an int?
 		if(hh > 23 || hh < 0 || isNaN(hh)){return false;}
 
 		var mm = parseInt($('mm_to').value) 	//is an int?
 		if(mm > 59 || mm < 0 || isNaN(mm)){return false;}
 		
 		var ss = parseInt($('ss_to').value) 	//is an int?
 		if(ss > 59 || ss < 0 || isNaN(ss)){return false;}
 		
 		var ff = parseInt($('ff_to').value) 	//is an int?
 		if(ff > 24 || ff < 0 || isNaN(ff)){return false;}
 		
 	}
 	
 	return true;
 }
 
 
 /*
  * reload tree data, using selected filter options
  */
 function refreshTree(options){
 	
 	tree.loader = new Ext.tree.TreeLoader({
        dataUrl: '../cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl?',
        //post parameters to pass to script
 		baseParams: {						
 			formatIn: options['format'],
 			tStartIn: options['fromtime'],
 			tEndIn: options['totime'],
 			keywordsIn: options['searchtext'],
 			projectIn: options['projname'],
 			rangeIn: options['range'],
 			dayIn: options['day']
 		}
 		
	});
 	
 	currentFormat = options['format']; 
 	
	tree.getRootNode().reload();	//reload the root
	tree.getRootNode().expand();	//expand the root
	if(dndMode){dropBox.getRootNode().expand();}
 }
 
 
 /*
  * Set aaf export options to the supplied json settings
  */
 function setExportDefaults(json){
 	
 	//cookie derived
 	if(json['fromcookie'] === 'TRUE'){
 		if(debug){alert('settings from cookie');}
 	
	 	if(json['FCP'] === 'TRUE'){$('format').value = 'fcp';}
			else {$('format').value = 'avid';}
		$('dircut').checked = (json['DirCut'] === 'TRUE');
		$('dircutaudio').checked = (json['AudioEdit'] === 'TRUE');
		$('exportdir').selectedIndex = json['ExportDir'];
		$('fnameprefix').value = json['FnamePrefix'];
		$('longsuffix').checked = (json['LongSuffix'] === 'TRUE');
		$('editpath').value = json['EditPath'];
//		$('dirsource').value = json['DirSource'];
 	}
 	
 	//server derived (json elements are object based)
 	else{
 		if(debug){alert('settings from server');}
 		
 		//set aaf export options to saved defaults
		$('format').value = json.FCP;
//		$('dircut').checked = (json.DirCut === 'true');
//		$('dirsource').value = json.DirSource;					//database storage of director's cut has replaced file storage
		$('dircut').checked = (json.DirDB === 'true');			
		$('dircutaudio').checked = (json.AudioEdit === 'true');
		$('exportdir').selectedIndex = json.ExportDir;
		$('fnameprefix').value = json.FnamePrefix;
		$('longsuffix').checked = (json.long_suffix === 'true');
		$('editpath').value = json.EditPath;

 	}
 	
 }


/*
 * creates an html link to an output file
 */
function fileLink(filename){
	var filenameText = addSoftHyphens(filename);
	var html = "<a href=" + "/cgi-bin/ingex-modules/Material.ingexmodule/download.pl?fileIn=" + filename + ">" + filenameText + "</a>";
	return html;
}


/*
 * add soft hyphens to a long piece of text (eg urls) so it can be split
 */
function addSoftHyphens(text){
	var next;
	var out = "";
	
	for(var i=0; i<text.length; i+=1){
		next = text.substring(i,i+1);
		out += "&shy;"+next;
	}	
	
	return out;
}


/*
 * an error occurred when loading tree data
 */ 
function treeLoadException(loader, node, response){
	var title = "Communications Error!";
	var message = 'Error when loading tree data';
	show_static_messagebox(title, message);
	insole.error(message);
}


/*
 * search for a key word in clip names/descriptions
 */
function keywordSearch(){
	submitFilter();	//apply filter
}


/*
 * Calculate the number of materials on supplied path
 * Can contain both nodes which contain all the original materials and those which only contain some of them
 */
function getMaterialCount(n){
	var count = 0;
	var node = n;
	
	if(debug){alert('getmaterialcount');}
	if(node instanceof Ext.tree.AsyncTreeNode){
		if(!node.attributes['leaf'] && !node.isLoaded()){
			//there are unloaded children, so this path will contain all original materials
			
			//add this path to count
			return node.attributes['materialCount'];
		}
	}
	if(node.attributes['leaf']){
		//this is a material leaf
		
		//add 1 to count
		return 1;
	}
	else{
		//this is a loaded folder, which contians some children
		
		//investigate each child
		var children = node.childNodes;
		
		for(var i=0; i<children.length; i++){
			if(!children[i].attributes){break;}
			count += getMaterialCount(children[i]);
		}
		
		return count;
	}
}


/*
 * delete link was clicked
 */
function deleteClicked(dom){
	if(toDelete != ''){
		//still packages waiting to be deleted
		alert("Still processing a deletion request - wait a few seconds and retry");
		return;
	}
	if(dom == 'src'){
		delNodes = selectedSrcNodes;
		deleteMaterials(dom, delNodes);
	}
	else if(dom == 'dest'){
		delNodes = selectedDestNodes;
		deleteMaterials(dom, delNodes);
	}
}


// delete all
function deleteAllClicked(dom){
	if(toDelete != ''){
		//still packages waiting to be deleted
		alert("Still processing a deletion request - wait a few seconds and retry");
		return;
	}	
	if(dom == 'src'){
		delNodes = rootNode.childNodes;
		deleteMaterials(dom, delNodes);
	}
	else if(dom == 'dest'){
		delNodes = new Array();
		delNodes[0] = dbRootNode;
		deleteMaterials(dom, delNodes);
	}
}


function deleteMaterials(dom, delNodes){
	if(dom == 'src'){
		if(delNodes.length > 0){
			var count = 0;
			
			for(var i=0; i<delNodes.length; i++)
			{
				if(!delNodes[i].attributes){break;}
				
				count += getMaterialCount(delNodes[i]);
			}
			
			var response = confirm("About to permanently delete " + count + " materials packages! Are you sure?");
			
			if(response){
				//delete packages from database - will remove from tree when refreshed on callback
				toDelete = delNodes;
				delCall(delNodes, count);
			}
			else{
				//do nothing
			}
		}
	}
	else if(dom == 'dest'){
		if(delNodes.length > 0){
			var count = 0;
			
			for(var i=0; i<delNodes.length; i++)
			{
				if(!delNodes[i].attributes){break;}
				
				count += getMaterialCount(delNodes[i]);
			}
			
			var response = confirm("Remove " + count + " materials from selection?");
			
			if(response){
				//yes
				
				//remove from tree
				removeNodesFromTree('dest', delNodes);
			}
		}
	}
}


//TODO - copy to external media
function copyToExtMedia(dom){
	alert('Not implemented yet');
}


/*
 * removes node path from tree, updates material count and refreshes view
 */
function removeNodesFromTree(dom, nodes){
	if(dom == 'dest'){
		//update package size
		var count = 0;
		
		for(var i=0; i<nodes.length; i++)
		{
			if(!nodes[i].attributes){break;}
			
			count += getMaterialCount(nodes[i]);
		}
		
		destPkgSize -= count;
		setDestPkgSize(destPkgSize);
	}
	
	for(var i=0; i<nodes.length; i++){
		//[higher level nodes will be removed first]
		if(!nodes[i].attributes){break;}
		
		var next = nodes[i].attributes.name;
		if(debug){alert('removing ' + nodes[i].attributes.name);}
		
		//set 'full' parameter of all parents to false, since this branch is no longer complete
		var parent = nodes[i];
		while(parent.getDepth() > 0){
			parent = parent.parentNode;
			parent.attributes.full = false;
		}
		
		if(nodes[i].getDepth() == 0){	//this is a root node
			if(debug){alert("root");}
				//can't delete node itself, so delete children
				while(nodes[i].firstChild){
					nodes[i].firstChild.remove();
				}
//				var children = nodes[i].childNodes;
//				for(var j in children){
//					if(debug){alert("next child " + children[j].text);}
//					nodes[i].removeChild(children[j]);
//				}
			}
			else{
				nodes[i].remove();
			}
		}
}
