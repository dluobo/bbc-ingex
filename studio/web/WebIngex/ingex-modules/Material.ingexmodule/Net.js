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
 * Server request/reply functions
 * Various server call and reply functions that are required by materials interface
 *****************************************************************************************/
 
 /*
  * Perform count of selected folders or materials
  */
 function countSelection(options, dom){
 	var dataUrl = '../cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl';
 	var params = '?formatIn='+options['format']+'&tStartIn='+options['fromtime']+'&tEndIn='+options['totime']+'&keywordsIn='+options['searchtext']+'&countIn=1'+'&timeIn='+options['time']+'&dateIn='+options['date']+'&projectIn='+options['project'];
 	ajaxRequest(dataUrl, "GET", params, countSelCallback, dom);
 }
 
 
 /*
  * couts the number of packages that meet sent parameters
  */
 function countPackages(options){	//TODO: change to post request for consistency
 	var dataUrl = '../cgi-bin/ingex-modules/Material.ingexmodule/treeLoader.pl';
 	var params = '?formatIn='+options['format']+'&tStartIn='+options['fromtime']+'&tEndIn='+options['totime']+'&keywordsIn='+options['searchtext']+'&countIn=1'+'&projectIn='+options['projname']+'&rangeIn='+options['range']+'&dayIn='+options['day'];
 	ajaxRequest(dataUrl, "GET", params, countCallback);
 }
 
 
 /*
  * deletes the selected material packages
  */
function delCall(nodes, count){
	//TODO: Show wait 'Deleting' dialogue
	var message = "Removing " + count + " packages...";
	show_wait_messagebox("Deleting Packages", message);
	
	var opts = new Array();
	
	
	for(var i=0; i<selectedSrcNodes.length; i++)
	{
		var node = selectedSrcNodes[i];
		
		if(!node.attributes){break;}
		
		//is a leaf node
		if(node.attributes.leaf){
			
			opts.push({"materialId":node.attributes.id}); 
		}
		
		//is a folder
		else{
			if(debug){alert(node.attributes['id']);}
			var pp = getPathParams(node);
			
			opts.push(	{
							formatIn: pp['format'],
				 			tStartIn: pp['fromtime'],
				 			tEndIn: pp['totime'],
				 			keywordsIn: pp['searchtext'],
				 			projectIn: pp['project'],
				 			dateIn: pp['date'],
				 			timeIn: pp['time']
				 			
						});
		}
	}

	var json = 	{
					"typeIn": "delete",
					"packages": opts
				};
	var jsonText = JSON.stringify(json);
	var params = 'jsonIn=' + jsonText;
	
	var dataUrl = '../cgi-bin/ingex-modules/Material.ingexmodule/ajaxCall.pl?';
 	ajaxRequest(dataUrl, "POST", params, delCallback);	
}


/*
 * call create_aaf script
 */
function createAAF(jsonText){
	var dataUrl = '../cgi-bin/ingex-modules/Material.ingexmodule/createAAF.pl';
	var params = 'jsonIn=' + jsonText;
 	ajaxRequest(dataUrl, "POST", params, createAAFCallback);	
}


/*
 * load aaf export options from server
 */
function loadExportOptions(){
	var dataUrl = '../cgi-bin/ingex-modules/Material.ingexmodule/ajaxCall.pl';
	var json = 	{"typeIn": "get_aff_config"};
	var jsonText = JSON.stringify(json);
	var params = 'jsonIn=' + jsonText;
 	ajaxRequest(dataUrl, "POST", params, loadExportCallback);	
}

  
 /*
  * tester callback - output reply to console
  */
 function testCallback(data){
 	insole.log(data);
 }
 
 
/*
 * callback for count selection request
 */
function countSelCallback(data, dom){
	//error
	if (data.match('^err~')){
		var errorText = data.substring(4);
		show_static_messagebox("Error Counting Materials", errorText);
	}
	
	else {
		var count = data.slice(data.indexOf(":")+1, data.indexOf("}"));
	 	
	 	if(dom == 'dest'){
	 		//add to total package size
	 		setDestPkgSize(destPkgSize + parseInt(count));
	 	}
	 	
	 	else{
	 		//show size of selected folder
		 	$('status_bar_src').innerHTML = '"' + selectedSrcNode.attributes['name'] + '" selected, containing ' + count + ' material items';
			noSelectedItems = count;
	 	}
	}
 }
 
 
 /*
  * callback for 'load export options' request
  */
 function loadExportCallback(data){

 	//all good
 	if (data.match('^ok~')){
		try{
			var json = eval('(' + data.substring(3) + ')'); 
		}
		catch(e){
			jsonParseException(e);
			return;
		}
		
		setExportDefaults(json);
		
		updateEnabledOpts();
	
 	}
 	
 	//error
	else if (data.match('^err~')){
		var errorText = data.substring(4);
		show_static_messagebox("Error Loading Default Settings", errorText);
	}
	
	//nothing or garbage received
	else {	
		var message = 'Communications error - invalid response from server';
	    show_static_messagebox("Error", message);
	}
 }
 
 
/*
 * callback for count of materials items matched
 */
function countCallback(data){
	//error
	if (data.match('^err~')){
		var errorText = data.substring(4);
		show_static_messagebox("Error Counting Materials", errorText);
	}
	
	else {
	 	var count = data.slice(data.indexOf(":")+1, data.indexOf("}"));
	 	$('filter_matches').innerHTML = count + " matching material items found";
		
		//set root node to count
		rootNode.attributes['materialCount'] = parseInt(count);
	}
}


/*
 * callback for createAAF script
 */
function createAAFCallback(data){

	//all good
	if (data.match('^ok~')){
		try{
			var json = eval('(' + data.substring(3) + ')'); 
		}
		catch(e){
			jsonParseException(e);
			return;
		}
		
		var totalClips = json.totalclips;
		var totalMulticamGroups = json.totalmultigroups;
		var totalDirectorsCutSequences = json.totaldircut;
		var fileName = json.filename;
			
		var message = '';
		message += "Totals" + "<BR>";
		message += "Clips: " + totalClips + "<BR>";
		message += "Multi-Camera Clips: " + totalMulticamGroups + "<BR>";
		message += "Director's Cut Sequences: " + totalDirectorsCutSequences + "<BR>";
					
		for(var i in json.filenames){
			if(json.filenames[i].file){message += "Url " + (parseInt(i)+1) + ": " + fileLink(json.filenames[i].file) + "<br>";}
		}
		
		show_static_messagebox("Created Package", message);
	}
	
	//error
	else if (data.match('^err~')){
		var errorText = data.substring(4);
		show_static_messagebox("Error Creating Package", errorText);
	}
	
	//nothing or garbage received
	else {	
		var message = 'Communications error - invalid response from server';
	    show_static_messagebox("Error", message);
	}
	
}


/*
 * callback for delete file
 */
function delCallback(data){
	//all good
	if (data.match('^ok~')){
		var message = data.substring(3);
		show_static_messagebox("Packages Deleted", message);
		submitFilter(); //refresh tree
		tree.getRootNode().expand();	//expand the root
	}
	
	//error
	else if (data.match('^err~')){
		var errorText = data.substring(4);
		show_static_messagebox("Error Deleting Materials", errorText);
	}
	
	//nothing or garbage received
	else {	
		var message = 'Communications error - invalid response from server';
	    show_static_messagebox("Error", message);
	}
	
	toDelete = '';	//clear buffer
}


/*
 * an error occurred when parsing json response
 */ 
function jsonParseException(e){
	//error parsing data
	var message = 'JSON parse error - invalid response from server';
	show_static_messagebox("Error", message);
	insole.error("Exception thrown:   Name: "+e.name+"   Message: "+e.message);
}