#!/usr/bin/perl -wT

# Copyright (C) 2008  British Broadcasting Corporation
# Author: Philip de Nier <philipn@users.sourceforge.net>
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
use ingexconfig;
use prodautodb;
use db;
use datautil;
use htmlutil; use ingexhtmlutil;;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();


if (!defined param('id'))
{
    return_error_page("Missing 'id' parameter");
}

if (defined param('Cancel'))
{
    redirect_to_page("sourcecf.pl");
}

my $scfId = param('id');
my $errorMessage;

my $recLocs = prodautodb::load_recording_locations($dbh) or 
    return_error_page("failed to load recording locations: $prodautodb::errstr");

my $scf = load_source_config($dbh, $scfId) or
    return_error_page("failed to find source config with id=$scfId from database: $prodautodb::errstr");

    
if (defined param('Reset'))
{
    Delete_all();
}
elsif (defined param('Done'))
{
    if (!($errorMessage = validate_params($scf, $recLocs)))
    {
        $scf->{"config"}->{"NAME"} = param("name");
        $scf->{"config"}->{"TYPE_ID"} = param("type");
        
        if ($scf->{"config"}->{"TYPE_ID"} == $db::sourceType{"LiveRecording"})
        {
            $scf->{"config"}->{"LOCATION_ID"} = param("recloc");
            $scf->{"config"}->{"SPOOL"} = undef;
        }
        else
        {
            $scf->{"config"}->{"LOCATION_ID"} = undef;
            $scf->{"config"}->{"SPOOL"} = param("tapenum");
        }

        my @paramNames = param();
        
        foreach my $paramName (@paramNames)
        {
            if ($paramName =~ /^trackname-(\d+)$/)
            {
                my $foundTrack = 0;
                foreach my $track (@{$scf->{"tracks"}})
                {
                    if ($1 == $track->{"TRACK_ID"})
                    {
                        $foundTrack = 1;
                        $track->{"NAME"} = param($paramName);
                        last;
                    }
                }
                return "Error: unknown track with id = $1" if (!$foundTrack);
            }
        }
        
        if (!update_source_config($dbh, $scf))
        {
            # do same as a reset
            Delete_all();
            $scf = load_source_config($dbh, $scfId) or
                return_error_page("failed to find source config with id=$scfId from database: $prodautodb::errstr");
            
            $errorMessage = "failed to update source config to database: $prodautodb::errstr";
        }
        else
        {
            $scf = load_source_config($dbh, $scfId) or
                return_error_page("failed to reload saved source config from database: $prodautodb::errstr");
    
            redirect_to_page("sourcecf.pl");
        }
        
    }
}


return_edit_page($scf, $recLocs, $errorMessage);




sub validate_params
{
    my ($scf, $recLocs) = @_;
    
    return "Error: empty name" if (!defined param("name") || param("name") =~ /^\s*$/);
    
    return "Error: missing type" if (!defined param("type"));
    return "Error: invalid type value" if (param("type") != $db::sourceType{"LiveRecording"} &&
        param("type") != $db::sourceType{"Tape"});
        
    if (param("type") == $db::sourceType{"LiveRecording"})
    {
        return "Error: missing recording location" if (!defined param("recloc"));
        my $found = 0;
        foreach my $recLoc (@{$recLocs})
        {
            if ($recLoc->{'ID'} eq param('recloc'))
            {
                $found = 1;
                last;
            }
        }
        return "Error: Unknown recording location" if (!$found);
    }
    else
    {
        return "Error: Empty tape number" if (!defined param('tapenum') || 
            param('tapenum') =~ /^\s*$/);
    }
    
    my @paramNames = param();
    
    foreach my $paramName (@paramNames)
    {
        if ($paramName =~ /^trackname-(\d+)$/)
        {
            return "Error: empty track name for track $1" if (param($paramName) =~ /^\s*$/);

            my $foundTrack = 0;
            foreach my $track (@{$scf->{"tracks"}})
            {
                if ($1 == $track->{"TRACK_ID"})
                {
                    $foundTrack = 1;
                    last;
                }
            }
            return "Error: unknown track with id = $1" if (!$foundTrack);
        }
    }
    
    return undef;
}

sub return_edit_page
{
    my ($scf, $recLocs, $errorMessage) = @_;
    
    my $page = get_edit_content($scf, $recLocs, $errorMessage) or
        return_error_page("failed to fill in content for edit source config page");
       
    print header;
    print $page;
    
    exit(0);
}

sub get_edit_content
{
    my ($scf, $recLocs, $errorMessage) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Edit source group definition"));

    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    
    push(@pageContent, start_form({-id=>"ingexForm", -action=>"javascript:sendForm('ingexForm','editscf')"}));

    push(@pageContent, hidden("id", $scf->{"config"}->{"ID"}));

    my @topRows;

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Name:'), 
            textfield('name', $scf->{"config"}->{'NAME'})]),
        ]));

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Type:'), 
                popup_menu(
                    -name=>'type',
                    -values=>[$db::sourceType{"LiveRecording"}, $db::sourceType{"Tape"}],
                    -default=>$scf->{"config"}->{'TYPE_ID'},
                    -labels=>{$db::sourceType{"LiveRecording"} => "LiveRecording", 
                        $db::sourceType{"Tape"} => "Tape"})]),
        ]));

    my @values;
    my %labels;
    foreach my $recLoc (@{$recLocs})
    {
        push(@values, $recLoc->{'ID'});
        $labels{$recLoc->{'ID'}} = $recLoc->{'NAME'};
    }
    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Location:'), 
                popup_menu(
                    -name=>'recloc',
                    -default=>$scf->{"config"}->{'LOCATION_ID'},
                    -values=>\@values,
                    -labels=>\%labels)])
        ]));

    push(@topRows,  
        Tr({-align=>'left', -valign=>'top'}, [
            td([div({-class=>"propHeading1"}, 'Tape number:'), 
                textfield('tapenum', $scf->{"config"}->{'SPOOL'})]),
        ]));

        
    my @trackRows;
    push(@trackRows,
        Tr({-align=>'left', -valign=>'top'}, [
           th(['Id', 'Name', 'Type']),
        ]));
    
    foreach my $track (@{$scf->{'tracks'}})
    {
        push(@trackRows, 
            Tr({-align=>'left', -valign=>'top'}, [
                td([$track->{'TRACK_ID'}, 
                    textfield("trackname-$track->{'TRACK_ID'}", $track->{'NAME'}),
                    $track->{'DATA_DEF'}]),
            ]));
        
    }
    
    push(@topRows,
        Tr({-align=>'left', -valign=>'top'}, [
           td([div({-class=>"propHeading1"}, 'Tracks:'), 
            table({-class=>"borderTable"}, @trackRows)]),
        ]));


        
    push(@pageContent, table({-class=>"noBorderTable"},
        @topRows));
    
    
    push(@pageContent, 
        p(
            submit({-onclick=>"whichPressed=this.name", -name=>"Done"}), 
            submit({-onclick=>"whichPressed=this.name", -name=>"Reset"}),
            submit({-onclick=>"whichPressed=this.name", -name=>"Cancel"}),
        )
    );

    push(@pageContent, end_form);

    
    return join("", @pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


