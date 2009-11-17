#!/usr/bin/perl -wT

# Copyright (C) 2009  British Broadcasting Corporation
# Author: Sean Casey <seantc@users.sourceforge.net>
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
#


#
# Materials page - displays a browsable tree containing a heirachal view of all materials.
#
# Materials can be filtered depending on a number of user-selectable parameters and selected materials
# can be packaged and exported to avid and final cut formats.
#

use strict;

use lib ".";
use lib "../../ingex-config";

use prodautodb;
use db;
use ingexhtmlutil;
use ingexconfig;
use materialconfig;

use CGI::Pretty qw(:standard);

my $dbh = prodautodb::connect(
		$ingexConfig{"db_host"}, 
		$ingexConfig{"db_name"},
		$ingexConfig{"db_user"}, 
		$ingexConfig{"db_password"})
 	or die();

my $main_browser = get_main_browser();

print header;
print $main_browser;

exit(0);


#
# Create html for main browser panel containing the media files
# Generates a tree/grid browser using ext components
#
sub get_main_browser {
	# get video resolution formats
	my $vresIds = prodautodb::load_video_resolutions($dbh)
	  	or return_error_page(
			"failed to load video resolutions from the database: $prodautodb::errstr"
	  	);

	my $val      = "";
	my $name     = "";
	my $fmt_opts = "<OPTION value='-1'>[any resolution]</OPTION>";

	foreach my $vres ( @{$vresIds} ) {
		$val      = $vres->{'ID'};
		$name     = $vres->{'NAME'};
		$fmt_opts = $fmt_opts . "<OPTION value='$val'>$name</OPTION>";
	}

	# get project names
	my $projnames = db::load_projects($dbh)
	  	or return_error_page(
			"failed to load video resolutions from the database: $prodautodb::errstr"
	  	);

	my $id = "";
	$name = "";
	my $proj_opts = "<OPTION value='-1'>[any project]</OPTION>";

	foreach my $proj ( @{$projnames} ) {
		$id        = $proj->{'ID'};
		$name      = $proj->{'NAME'};
		$proj_opts = $proj_opts . "<OPTION value='$id'>$name</OPTION>";
	}

	# panels displaying properties of selected package
	my $package_details1 = get_package_details('src');
	my $package_details2 = get_package_details('dest');

	# get export directories
	my $dest_directories = '';
	my $defaultSendTo;
	my @allExportDirs = get_all_avid_aaf_export_dirs();

	foreach my $exportDir (@allExportDirs) {
		if ( -e $exportDir->[1] ) {
			my $label = $exportDir->[0] . " ($exportDir->[1])";
			my $value = $exportDir->[0];
			$dest_directories .= "<option value='$value'>$label</option>";
			if ( !defined $defaultSendTo ) {
				$defaultSendTo = $value;
			}
		}
	}


# Director's cut source is now sql-based

#	# get director's cut databases
#	my $defaultDCSource;
#	my $dc_directories = '';
#
#	foreach my $dctDb ( get_all_directors_cut_dbs() ) {
#		if ( -e $dctDb->[1] ) {
#			my $label = $dctDb->[0] . " ($dctDb->[1])";
#			my $value = $dctDb->[1];
#			$dc_directories .= "<option value='$value'>$label</option>";
#			if ( !defined $defaultDCSource ) {
#				$defaultDCSource = $value;
#			}
#		}
#	}

#						<li>
#							<label for="dirsource">
#								Director's cut source
#							</label> 
#							<select id="dirsource" default="$defaultDCSource" class="item">
#								$dc_directories
#							</select>
#						</li>						Director's cut source is now sql-based

	my $content;

	if($ingexConfig{"drag_drop"} eq 'true')
	{
	
	# html content for main page body
	$content = <<ENDHTML;
		
		<body>
			
			<h1>Material Browser</h1>		
		
			<div class="materials_browser">
				<h3>Materials Browser - <i>select required packages</i></h3>
				
				
	
												
				<!---		EXAMPLE TEMPLATE FOR LAYING OUT FORM ELEMENTS
							<fieldset class="table">
							<legend>Time Range</legend>
								<ol>
									<li>
										<label for="item1">
										fewfew
										</label>
										<div id="item1" class="item">
											regvre
										</div>
									</li>
								</ol>
							</fieldset>	--->
							
							
							
						<fieldset class="table">
							<legend>Filter Options</legend>
								<ol>
									<li>
										<label for="proj_name">
											Project Name
										</label>
										<select id="proj_name" name="proj_name" class="item" default="0">
											$proj_opts
										</select>
									</li>
									
									<li>
										<label for="search_text">
											Search Terms
										</label>
										<input type="text" id="search_text" class="item" />
									</li>
								
									<li>
										<label for="v_res">
											Video Resolution
										</label>
										<select id="v_res" class="item" default="8" />
											$fmt_opts
										</select>
									</li>
								</ol>
							</fieldset>
							
							<fieldset class="table">
								<legend>Time Range</legend>
								<ol>
									<li>
										<label for="dummy">
											<input type="radio" name="time_type" id="all_time_radio" value="all" onclick="allTimesSet()" checked="checked" />
											All times
										</label>
										<div id="dummy" class="var_container"></div>
									</li>
									
									<li>
										<label for="dummy2">
											<input type="radio" name="time_type" id="time_period_radio" value="range" onclick="dayFilterSet()" />
											Time range: 
										</label>
										<div id="dummy2" class="var_container"></div>
									</li>
									
									<li>
										<label for="range_in" class="label_r">
											Period
										</label>
										<select id="range_in" class="item" disabled="true">
											<option value="1">last 10 minutes</option>
											<option value="2">last 20 minutes</option>
											<option value="3">morning (00:00 - 11:59)</option>
											<option value="4">afternoon (12:00 - 23:59)</option>
											<option value="5">all day</option>
											<option value="6">2 days</option>
										</select>
									</li>
									
									<li>
										<label for="day_in" class="label_r">
											Day
										</label>
										<select id="day_in" class="item" disabled="true">
											<option value="1">today</option>
											<option value="2">yesterday</option>
											<option value="3">2 days ago</option>
											<option value="4">3 days ago</option>
										</select>
									</li>
									
								
									<li>
										<label for="dummy3">
											<input type="radio" name="time_type" id="time_range_radio" value="period" onclick="dateFilterSet()" /> 
											Within the period:
										</label>
										<div id="dummy3" class="var_container">
										</div>
									</li>
								
								
									<li>
										<label for="datetime_from" class="label_r">
											From
										</label>
										<div id="datetime_from" class="var_container">
											<div class="var_container">
												<div id="from_cal"></div>	<!--- Ext calendar popup ---> 
											</div>			
											<div id="from_time" class="var_container">
												<input type="textbox" name="hh_from" id="hh_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="mm_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="ss_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="ff_from" class="time_in" size="2" value="00" disabled="true" />
											</div>
											<div class="var_container"> [hh:mm:ss:ff]</div>
											
											<!--- Verification error --->			
											<div id="time_range_err" class="time_range_err">
												* Timecode must be in the format hh:mm:ss:ff
											</div>		
										</div>
									</li>
									<li>
										<label for="datetime_to" class="label_r">
											To
										</label>
										<div id="datetime_to" class="var_container">
											<div class="var_container">
												<div id="to_cal"></div>	<!--- Ext calendar popup ---> 
											</div>			
											<div id="to_time" class="var_container">
												<input type="textbox" id="hh_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="mm_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="ss_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="ff_to" class="time_in" size="2" value="00" disabled="true">
											</div>
											<div class="var_container"> [hh:mm:ss:ff]</div>
										</div>
									</li>
								</ol>
							</fieldset>	
							
		
		
		
		 
							<fieldset class="noborder">
								<!--- Submit --->
								<div class="var_container">
									<input type="button" value="Load Clips" onclick="submitFilter()" />
								</div>
								<div class="var_container">
									<!--- Search matches ---> 
									<div nane='filter_matches' id='filter_matches' class="filter_matches"></div>
								</div>
							</fieldset>
												 
							
											
				<div class="container">
					<div id="tree" class="tree"></div>
					<div id="metadata" class="metadata">
						$package_details1
					</div>
				</div>
				
				<div id="status_bar_src" class="status_bar_src">
				</div>
			</div>
			
			
			<br/>
						
			<div class="aaf_selection">
				<h3>Materials Selection - <i>drag materials here</i></h3>
				
				<div class="container2">
						<div id="aafbasket" class="aafbasket"></div>
						<div id="metadata" class="metadata">
							$package_details2
						</div>
						
				</div>
				
				<div id="status_bar_dest" class="status_bar_src">
				</div>
				
				
				
									
									
				<fieldset class="table">
					<legend>Export Options</legend>
					<ol>
						<li>
							<label for="format">
								Format
							</label> 
							<select id="format" class="item" onchange="updateEnabledOpts()">
								<option value="avid">Avid</option>
								<option value="fcp">Final Cut Pro</option>
							</select>
						</li>
						
						<li>
							<label for="editpath">
								Editing directory path
							</label> 
							<input type="text" id="editpath" class="item" />
							<div class="var_container"> FCP only</div>
						</li>
					</ol>
				</fieldset>
				
				<fieldset class="table">
					<legend>Director's cut</legend>
					<ol>
						<li>
							<label for="dummy">
								<input type="checkbox" id="dircut" onclick="updateEnabledOpts()"/>
								Add director's cut
							</label>
							<div id="dummy" class="var_container">
							</div>
						</li>
						
						<li>
							<label for="dummy">
								<input type="checkbox" id="dircutaudio" onchange="updateEnabledOpts()"/>
								Add audio edits
							</label>
							<div id="dummy" class="var_container">
							</div>
						</li>
					</ol>
				</fieldset>
				
				
				<fieldset class="table">
					<legend>Output</legend>
					<ol>
						<li>
							<label for="fnameprefix">
								Filename prefix
							</label> 
							<input id="fnameprefix" type="text" class="item" /> 
							<div class="var_container">
								<input type="checkbox" id="longsuffix"> Add long suffix
							</div>
						</li>
						
						<li>
							<label for="exportdir">
								Destination directory
							</label> 
							<select id="exportdir" default="$defaultSendTo" class="item">
								$dest_directories
							</select>
						</li>
					</ol>
				</fieldset>
			
			<fieldset class="noborder">
				<div class="var_container">
					<input type="button" value="Create Package" onclick="packageAAF()" />
					<input type="checkbox" id="save" /> Save these options as global defaults
				</div>
			</fieldset>
			
			</div>
			
			<fieldset class="noborder">
				<div class="centre_container">
					<span onclick="javascript:getContent('instructions')" onMouseOver="this.style.cursor='pointer'">
						How do I use this page?
					</span> |
					<span onclick="javascript:getTab('OldMaterial', false, true, true)" onMouseOver="this.style.cursor='pointer'">
						View in basic HTML
					</span> 
				</div>
			</fieldset>
			
			<br/>

		</body>

		<div class='displayOptions'><h5>Display Options:</h5> <a href='javascript:toggleTC()'>Toggle timecode frame display</a></div>		
ENDHTML
	}
	
	
	# non drag-drop version
	else
	{
			
	# html content for main page body
	$content = <<ENDHTML;
		
		<body>
			
			<h1>Material Browser</h1>		
		
			<div class="materials_browser">
				
				<div class="filter_container">
				
					<h3>1) Clip Selection - <i>select required clips</i></h3>
					
					
		
													
					<!---		EXAMPLE TEMPLATE FOR LAYING OUT FORM ELEMENTS
								<fieldset class="table">
								<legend>Time Range</legend>
									<ol>
										<li>
											<label for="item1">
											fewfew
											</label>
											<div id="item1" class="item">
												regvre
											</div>
										</li>
									</ol>
								</fieldset>	--->
								
								
								
							<fieldset class="table">
								<legend>Filter Options</legend>
									<ol>
										<li>
											<label for="proj_name">
												Project Name
											</label>
											<select id="proj_name" name="proj_name" class="item" default="0">
												$proj_opts
											</select>
										</li>
										
										<li>
											<label for="search_text">
												Search Terms
											</label>
											<input type="text" id="search_text" class="item" />
										</li>
									
										<li>
											<label for="v_res">
												Video Resolution
											</label>
											<select id="v_res" class="item" default="8" />
												$fmt_opts
											</select>
										</li>
									</ol>
								</fieldset>
								
								<fieldset class="table">
									<legend>Time Range</legend>
									<ol>
										<li>
											<label for="dummy">
												<input type="radio" name="time_type" id="all_time_radio" value="all" onclick="allTimesSet()" checked="checked" />
												All times
											</label>
											<div id="dummy" class="var_container"></div>
										</li>
										
										<li>
											<label for="dummy2">
												<input type="radio" name="time_type" id="time_period_radio" value="range" onclick="dayFilterSet()" />
												Time range: 
											</label>
											<div id="dummy2" class="var_container"></div>
										</li>
										
										<li>
											<label for="range_in" class="label_r">
												Period
											</label>
											<select id="range_in" class="item" disabled="true">
												<option value="1">last 10 minutes</option>
												<option value="2">last 20 minutes</option>
												<option value="3">morning (00:00 - 11:59)</option>
												<option value="4">afternoon (12:00 - 23:59)</option>
												<option value="5">all day</option>
												<option value="6">2 days</option>
											</select>
										</li>
										
										<li>
											<label for="day_in" class="label_r">
												Day
											</label>
											<select id="day_in" class="item" disabled="true">
												<option value="1">today</option>
												<option value="2">yesterday</option>
												<option value="3">2 days ago</option>
												<option value="4">3 days ago</option>
											</select>
										</li>
										
									
										<li>
											<label for="dummy3">
												<input type="radio" name="time_type" id="time_range_radio" value="period" onclick="dateFilterSet()" /> 
												Within the period:
											</label>
											<!--- Verification error --->			
												<div id="time_range_err" class="time_range_err">
													* Timecode must be in the format hh:mm:ss:ff
												</div>	
											<div id="dummy3" class="var_container">
											</div>
										</li>
									
									
										<li>
											<label for="datetime_from" class="label_r">
												From
											</label>
											<div id="datetime_from" class="var_container">
												<div class="var_container">
													<div id="from_cal"></div>	<!--- Ext calendar popup ---> 
												</div>			
												<div id="from_time" class="var_container">
													<input type="textbox" name="hh_from" id="hh_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="mm_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="ss_from" class="time_in" size="2" value="00" disabled="true" />:<input type="textbox" id="ff_from" class="time_in" size="2" value="00" disabled="true" />
												</div>
												<div class="var_container"> [hh:mm:ss:ff]</div>
												
													
											</div>
										</li>
										<li>
											<label for="datetime_to" class="label_r">
												To
											</label>
											<div id="datetime_to" class="var_container">
												<div class="var_container">
													<div id="to_cal"></div>	<!--- Ext calendar popup ---> 
												</div>			
												<div id="to_time" class="var_container">
													<input type="textbox" id="hh_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="mm_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="ss_to" class="time_in" size="2" value="00" disabled="true">:<input type="textbox" id="ff_to" class="time_in" size="2" value="00" disabled="true">
												</div>
												<div class="var_container"> [hh:mm:ss:ff]</div>
											</div>
										</li>
									</ol>
								</fieldset>	
								 
								<fieldset class="noborder">
									<!--- Submit --->
									<div class="var_container">
										<input type="button" value="Load Clips" onclick="submitFilter()" />
									</div>
									<div class="var_container">
										<!--- Search matches ---> 
										<div nane='filter_matches' id='filter_matches' class="filter_matches"></div>
									</div>
								</fieldset>
								
					</div>		
				
				
				
				<div class="export_container">
					<h3>2) Output - <i>create package</i></h3>
					<fieldset class="table">
						<legend>Export Options</legend>
						<ol>
							<li>
								<label for="format">
									Format
								</label> 
								<select id="format" class="item" onchange="updateEnabledOpts()">
									<option value="avid">Avid</option>
									<option value="fcp">Final Cut Pro</option>
								</select>
							</li>
							
							<li>
								<label for="editpath">
									Editing directory path
								</label> 
								<input type="text" id="editpath" class="item" />
								<div class="var_container"> FCP only</div>
							</li>
						</ol>
					</fieldset>
					
					<fieldset class="table">
						<legend>Director's cut</legend>
						<ol>
							<li>
								<input type="checkbox" id="dircut" onclick="updateEnabledOpts()"/>
								Add director's cut
							</li>
							
							<li>
								<input type="checkbox" id="dircutaudio" onchange="updateEnabledOpts()"/>
								Add audio edits
							</li>
						</ol>
					</fieldset>
					
					
					<fieldset class="table">
						<legend>Output</legend>
						<ol>
							<li>
								<label for="fnameprefix">
									Filename prefix
								</label> 
								<input id="fnameprefix" type="text" class="item" /> 
								<div class="var_container">
									<input type="checkbox" id="longsuffix"> Long suffix
								</div>
							</li>
							
							<li>
								<label for="exportdir">
									Destination directory
								</label> 
								<select id="exportdir" default="$defaultSendTo" class="item">
									$dest_directories
								</select>
							</li>
						</ol>
					</fieldset>
				
				<fieldset class="noborder">
					<div class="var_container">
						<input type="button" value="Create Package" onclick="packageAAF()" />
					</div>
					<div class="var_container">
							<input type="checkbox" id="save" /> Save these options as local defaults
					</div>
					
				</fieldset>
			</div>
				

				
				
				<div class="container">
					<div id="tree" class="tree"></div>
					<div id="metadata" class="metadata">
						$package_details1
					</div>
				</div>
				
				<div id="status_bar_src" class="status_bar_src">
				</div>
			</div>
			
									

			
			
			<fieldset class="noborder">
				<div class="centre_container">
					<span onclick="javascript:getContent('instructions')" onMouseOver="this.style.cursor='pointer'">
						How do I use this page?
					</span> |
					<span onclick="javascript:getTab('OldMaterial', false, true, true)" onMouseOver="this.style.cursor='pointer'">
						View in basic HTML
					</span> 
				</div>
			</fieldset>
			
			<br/>

		</body>
		
		<div class='displayOptions'><h5>Display Options:</h5> <a href='javascript:toggleTC()'>Toggle timecode frame display</a></div>
ENDHTML
		
	}
	
	return $content;
}


#
# create a panel which can display properties of selected materials package, such as track names and urls and delete options.
#
sub get_package_details {
	my ($id) = @_;

	my $package_details;
	my $title;

	if ( $id eq 'src' ) {
		$title = "Source Details";    # source tree
	}
	else {
		$title = "Output Package";    # destination tree
	}

	$package_details = <<ENDHTML;
						<div id="m_text" class="m_text">
							<h2>
								$title
							</h2>
							<br/>
							
							<h4>
								Material Actions: 	<!--- TODO: make these pluggable for future expansion - should be loaded from array/refer to functions --->
							</h4>
							<span onclick='deleteClicked("$id")' onMouseOver="this.style.cursor='pointer'">> delete</span><br/>
							<span onclick='deleteAllClicked("$id")' onMouseOver="this.style.cursor='pointer'">> delete all</span><br/>
							<!---span onclick='copyToExtMedia("$id")' onMouseOver="this.style.cursor='pointer'">> copy to external media</span><br/>--->		
							<br/>
							
							<div id='package_$id' style='visibility: hidden;'>
								<h4>
									Tracks:
								</h4>
								<div id='tracks_$id'></div>
								<br/>
								<h4>
									Description:
								</h4>
								<div id="meta_description_$id"></div>
								<br/>
								<h4>
									Comments:
								</h4>
								<div id="meta_comments_$id"></div>
							</div>
						</div>
ENDHTML

	return $package_details;
}


END {
	prodautodb::disconnect($dbh) if ($dbh);
}

