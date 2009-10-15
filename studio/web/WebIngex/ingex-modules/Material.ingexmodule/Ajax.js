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
 * Simple AJAX library
 * Allows for execution of HTTP POST and GET requests
 *****************************************************************************************/
 
/*
 * ajax request - creates a post or get ajax request
 */
function ajaxRequest(http_url, type, params, callback){
	ajaxRequest(http_url, callback, '');	// no callback parameters
}
function ajaxRequest(http_url, type, params, callback, opts){
	//from: http://www.jibbering.com/2002/4/httprequest.html
	var xmlhttp=false;
	/*@cc_on @*/
	/*@if (@_jscript_version >= 5)
	// JScript gives us Conditional compilation, we can cope with old IE versions.
	// and security blocked creation of the objects.
	 try {
	  xmlhttp = new ActiveXObject("Msxml2.XMLHTTP");
	 } catch (e) {
	  try {
	   xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
	  } catch (E) {
	   xmlhttp = false;
	  }
	 }
	@end @*/
	if (!xmlhttp && typeof XMLHttpRequest!='undefined') {
		try {
			xmlhttp = new XMLHttpRequest();
		} catch (e) {
			xmlhttp=false;
		}
	}
	if (!xmlhttp && window.createRequest) {
		try {
			xmlhttp = window.createRequest();
		} catch (e) {
			xmlhttp=false;
		}
	}
	
	if(xmlhttp){
		sendRequest(xmlhttp, http_url, type, params, callback, opts);
	}	
}


/*
 * send the http GET or POST request
 */
function sendRequest(req, http_url, type, params, callback, opts){

	//define a timeout value
	var ajaxTimeoutCounter = self.setTimeout(function(){ajaxTimeout(req)}, timeout_period);

	//resonse - called when http request returns
	function httpResponse(){
		if (req.readyState==4) {		//wait until downloaded
			//clear timeout
			if(ajaxTimeoutCounter){
				self.clearTimeout(ajaxTimeoutCounter);
			}
		
			if (req.status == 200) {
				//all ok
				if(opts){ callback(req.responseText, opts); }
				else{ callback(req.responseText); }
			}
			else if (req.status == 404) {
				//page not found
				var message = "Page at url: " + req + " could not be found<BR>";
				show_static_messagebox("Error", message);
			}
			else if (req.status == 500) {
				//internal server fault
				var message = "Internal server error when calling page: " + req + "<BR>";
				show_static_messagebox("Error", message);
			}
			else{
				//a different error occurred
				var message = "An unknown error occurred<BR>";
				show_static_messagebox("Error", message);
			}
		}
	}
	
	
	if(type == "GET"){
		req.open("GET",http_url + params,true);
		req.onreadystatechange = function(){httpResponse()};
		req.send(null);
	}	
	else if(type== "POST"){
		req.open("POST", http_url, true);
		
		req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
		req.setRequestHeader("Content-length", params.length);
		req.setRequestHeader("Connection", "close");
		
		req.onreadystatechange = function(){httpResponse()};
		req.send(params);	//json-encoded params
	}
	
}


/*
 * an ajax timeout occurred
 */
function ajaxTimeout(req) {
	req.abort();	//abort request
	
	//error - timeout occured
	var title = "Communications error!";
	var message = "AJAX timeout occurred when communicating with server" + "<BR>";
	show_static_messagebox(title, message);
	insole.error("AJAX timeout occurred when communicating with server.");
}
