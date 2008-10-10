#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Rowan de Pomerai <rdepom@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
 
use strict;

use CGI::Pretty qw(:standard);

use lib ".";
use lib "../../ingex-config";
use ingexhtmlutil;



my $page = get_page_content() 
    or return_error_page("Failed to fill in content for this page.");
   
print header;
print $page;

exit(0);



sub get_page_content
{
    return "
	<h1> Production Logging</h1>
	
	<div id='logging'>
	<div id='leftColumn'>	
		<div id='programmeInfo'>
			<h1>Programme</h1>
			Series: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::series\");ingexAddToHistory(\"series\");'>edit</a>)</span>
			<br />
			<select name='series' class='progInfoFormElement' id='seriesSelector' onChange='ILupdateProgrammes()'>
				<option value='0'>Loading...</option>
			</select>
			<br />
			Programme: <span class='small'>(<a href='javascript:ingexUrlChange(\"Setup::prog\");ingexAddToHistory(\"prog\");'>edit</a>)</span>
			<br />
			<select name='series' class='progInfoFormElement' id='programmeSelector' onChange='ILloadNewProg()'>
			</select>
			<br />
			Producer:
			<br /><input type='text' id='producer' name='producer' class='progInfoFormElement' value='' disabled=true><br />
			Director:
			<br /><input type='text' id='director' name='director' class='progInfoFormElement' value='' disabled=true><br />
			PA:
			<br /><input type='text' id='pa' name='pa' class='progInfoFormElement' value='' disabled=true>
			<!--
				<div class='rightButtons'>
					<small><a class='simpleButton' href=''>Save Changes</a></small>
				</div>
			-->
		</div>
		
		<div id='recorderSelect'>
			<h1>Recorder</h1>
			<select name='recorder' class='progInfoFormElement' id='recorderSelector' onChange='ILtc.startIncrementing()'>
				<option value='0'>Loading...</option>
			</select>
		</div>
		
		<div id='currentTC'>
			<h1>Timecode</h1>
			<p class='center' id='tcDisplay'>-- : -- : -- : --</p>
		</div>
		
		<div id='actions'>
			<h1>Actions</h1>
			<p class='center'>
				<a class='simpleButton' href='javascript:ILimport();'>Import</a>
				<a class='simpleButton' href='javascript:ILprint();'>Print</a>
			</p>
		</div>
	</div>
	
		<div id='takes'>
			<h1>Recorded Takes</h1>
			<div id='takesList'></div>
			<div class='rightButtons'>
				<a class='simpleButton' href='javascript:ILloadNewProg();'>Refresh Data</a>
				<a class='simpleButton' href='javascript:ILnewNode();'>New Item</a>
				<a class='simpleButton' href='javascript:ILduplicateNode();'>Duplicate Item</a>
				<a class='simpleButton' href='javascript:ILdeleteNode();'>Delete Item</a>
			</div>
		</div>

		<div id='newTake'>
			<h1>New Take</h1>
			<table class='alignmentTable'><tr><td class='takeInfo'>
				<fieldset>
				<legend>
				Take Information
				</legend>
				<table class='alignmentTable'>
					<tr>
						<th>Recording Location:</th>
					</tr>
					<tr>
						<td>
							<select name='recordingLocation' class='progInfoFormElement' id='recLocSelector'>
								<option value='studioA'>Loading...</option>
							</select>
						</td>
					</tr>
					<tr>
						<th>Item/Shot:</th>
					</tr>
					<tr>
						<td>
							<select name='itemSelection' class='progInfoFormElement' id='itemSelector'>
								<option value='0'>Loading...</option>
							</select>
							<div class='rightButtons'>
								<a class='small simpleButton' href='javascript:ILselectPrevItem();'>prev</a>
								<a class='small simpleButton' href='javascript:ILselectNextItem();'>next</a>
							</div>
						</td>
					</tr>
				</table>
				</fieldset>
			</div>
			</td><td>
				<fieldset>
				<legend>
				Take Details
				</legend>
				<table class='alignmentTable'>
					<tr>
						<th>In:</th>
						<td><input type='text' name='in' class='progInfoFormElement' value='00:00:00:00' id='inpointBox'></td>
						<th style='width:100%'>Comments:</th>
					</tr>
					<tr>
						<th>Out:</th>
						<td><input type='text' name='out' class='progInfoFormElement' value='00:00:00:00' id='outpointBox'></td>
						<td rowspan='3'><textarea id='commentBox'></textarea></td>
					</tr>
					<tr>
						<th>Duration:</th>
						<td><input type='text' name='duration' class='progInfoFormElement' value='00:00:00:00' id='durationBox'></td>
					</tr>
					<tr>
						<th>Result:</th>
						<td id='resultText' class='noGood'>No Good</td>
					</tr>
				</table>
				</fieldset>
			</td></tr></table>
			<div class='spacer'>&nbsp;</div>
			<div class='leftButtons'>
				<a class='simpleButton' href='javascript:ILstartStop();' id='startStopButton'>Start</a>
			</div>
			<div class='leftExtraButtons'>
				<a class='simpleButton' href='javascript:ILsetGood();'>Good</a>
				<a class='simpleButton' href='javascript:ILsetNoGood();'>No Good</a>
			</div>
			<div class='rightButtons'>
				<a class='simpleButton' href='javascript:ILresetTake();'>Cancel</a>
				<a class='simpleButton' href='javascript:ILstoreTake();'>Store Take</a>
			</div>
		</div>
	</div>
	
	<div class='displayOptions'><h5>Display Options:</h5> <a href='javascript:toggleTC()'>Toggle timecode frame display</a></div>
	";
}


