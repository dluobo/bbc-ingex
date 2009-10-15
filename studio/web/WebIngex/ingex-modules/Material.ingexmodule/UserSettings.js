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
 * User settings
 * Utility class that uses cookies to store default ingex settings
 *****************************************************************************************/
 
var settingParams = [];
var doc;
var cookieName = "IngexSettings";
var expiry;

//constructor
function UserSettings(Document){
	doc = Document;
	
	//cookie expires long time in future
	var nextyear = new Date();
	nextyear.setFullYear(nextyear.getFullYear() + 5);
	expiry = nextyear.toGMTString();
	
	this.load();
}

//load the settings cookie
UserSettings.prototype.load = function(){
	var allcookies = doc.cookie;
	if(allcookies == ""){return;}
	
	//extract cookie values
	var myVals;
	var pos = allcookies.indexOf(cookieName);
	if(pos < 0){return;}
	pos += cookieName.length + 1;
	myVals = allcookies.substring(pos);
	var end = myVals.indexOf(";");
	myVals = myVals.substring(0, end);
	
	var pairs = myVals.split("&");
	var pair;
	var val;
	var key;
	
	//load each value-key property pair
	for(var i = 0; i < pairs.length; i++){
		pair = pairs[i].split(":");
		key = pair[0];
		val = pair[1];
		settingParams[key] = val;
	}
}

//save the settings to cookie
UserSettings.prototype.save = function(){
	var cookieVals = "";
	var newCookie = "";
	var val;
	
	for(var key in settingParams){
		if(typeof this[key] != 'function' || key != '' || key){
			val = settingParams[key];
			if(cookieVals != ""){
				cookieVals += "&";
			}
			cookieVals += key + ":" + unescape(val);
		}
	}

	newCookie += cookieName + "=" + cookieVals + ";"
	newCookie += "expires=" + expiry + ";"
	
	doc.cookie = newCookie;
}

//set a settings property
UserSettings.prototype.set = function(key, value){
	if(!key || key == ""){return;}
	
	settingParams[key] = escape(value);
}

//retrieve a settings property
UserSettings.prototype.get = function(key){
	if (settingParams[key]){
		return settingParams[key];
	}
	else{
		return;
	}
}

//retrieve array of all settings properties
UserSettings.prototype.getAll = function(){
	return settingParams;
}

//remove the settings cookie
UserSettings.prototype.remove = function(){
}