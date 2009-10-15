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
use lib "../../ingex-modules/Material.ingexmodule";

use ingexconfig;
use materialconfig;
use ingexhtmlutil;
use prodautodb;
use db;



my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();


my ($numRows) = (param("numrows") =~ /(\d+)/) if (defined param("numrows"));
my ($start) = (param("start") =~ /(\d+)/) if (defined param("start"));



my $max = get_material_count($dbh);
return_error_page("failed to get count of material in database: $prodautodb::errstr") if ($max < 0);

$numRows = $ingexConfig{"default_material_rows"} 
    if !defined $numRows || $numRows < 1 || $numRows > $ingexConfig{"max_material_rows"};
$start = 1 if !defined $start ||  $start < 1 || $start > $max;


my $page;
if ($max < 1)
{
    $page = get_page_content(undef, $numRows, 0, 0)
        or return_error_page("failed to fill in content for material page");
}
else
{
    my $material = load_material_1($dbh, $start - 1, $numRows)
        or return_error_page("failed to load $numRows material items, starting from $start, "
            . "from the database: $prodautodb::errstr");
        
    $page = get_page_content($material, $numRows, $start, $max)
        or return_error_page("failed to fill in content for material page");
}
   
print header;
print $page;

exit(0);


sub get_duration_string
{
    my ($duration) = @_;
    
    my ($hour, $min, $sec, $fr);
    
    $hour = int($duration / (60 * 60 * 25));
    $min = int(($duration % (60 * 60 * 25)) / (60 * 25));
    $sec = int((($duration % (60 * 60 * 25)) % (60 * 25)) / 25);
    $fr = int(((($duration % (60 * 60 * 25)) % (60 * 25)) % 25));
    
    my $durationString;
    if ($hour > 0)
    {
        $durationString = sprintf("%02d:%02d:%02d:%02d", 
            $hour, $min, $sec, $fr);
    }
    elsif ($min > 0)
    {
        $durationString = sprintf("%02d:%02d:%02d", 
            $min, $sec, $fr);
    }
    else
    {
        $durationString = sprintf("%02d:%02d", 
            $sec, $fr);
    }
        
    return $durationString;
}

sub get_timecode_string
{
    my ($position) = @_;
    
    my ($hour, $min, $sec, $fr);
    
    $hour = int($position / (60 * 60 * 25));
    $min = int(($position % (60 * 60 * 25)) / (60 * 25));
    $sec = int((($position % (60 * 60 * 25)) % (60 * 25)) / 25);
    $fr = int(((($position % (60 * 60 * 25)) % (60 * 25)) % 25));
    
    return sprintf("%02d:%02d:%02d:%02d", 
        $hour, $min, $sec, $fr);
}

# convert UTC to local time
sub get_local_time
{
    my ($creationDate) = @_;
    
    if (!defined $creationDate || length $creationDate == 0)
    {
        return "";
    }
    
    local $ENV{"PATH"} = "";
    
    my $cmd = "/bin/date -d \"$creationDate\"" . "Z +\"%Y-%m-%d %H:%M:%S\"";
    return `$cmd`;
}

sub merge_track_numbers
{
    my ($numberRanges, $number) = @_;

    my $index = 0;
    my $inserted = 0;
    foreach my $range (@{ $numberRanges })
    {
        if ($inserted)
        {
            if ($number == $range->[0] - 1)
            {
                # merge range with previous range
                $numberRanges->[$index - 1]->[1] = $range->[1];
                delete $numberRanges->[$index];
            }
            last;
        }
        else
        {
            if ($number == $range->[0] || $number == $range->[0] - 1)
            {
                # extend range to left
                $range->[0] = $number;
                $inserted = 1;   
            }
            elsif ($number == $range->[1] || $number == $range->[1] + 1)
            {
                # extend range to right
                $range->[1] = $number;
                $inserted = 1;   
            }
        }
        
        $index++;
    }
    
    if (!$inserted)
    {
        push(@{ $numberRanges }, [$number, $number]);
    }
}

sub get_page_content
{
    my ($material, $numRows, $start, $max) = @_;
    
    my $end = $start + $numRows - 1;
    $end = $max if ($end > $max);
    
    my $prevStart = 0;
    my $prevEnd;
    if ($start > 1)
    {
        $prevEnd = $start - 1;
        $prevStart = $prevEnd - $numRows + 1;
        $prevStart = 1 if ($prevStart < 1);
    }
    
    my $nextStart = 0;
    my $nextEnd;
    if ($end < $max)
    {
        $nextStart = $end + 1;
        $nextEnd = $nextStart + $numRows - 1;
        $nextEnd = $max if ($nextEnd > $max);
    }
    
    
    my @pageContent;
    
    push(@pageContent, h1("Essence material"));

    
    push(@pageContent, br());
    
    my @searchNav;
    
    push(@searchNav,  
        a({-href=>"javascript:getContentWithVars('index','start=1&numrows=$numRows')"}, "<<"),
    );
    
    if ($prevStart)
    {
        push(@searchNav,  
            a({-href=>"javascript:getContentWithVars('index','start=$prevStart&numrows=$numRows')"}, 
                "&nbsp;&nbsp;<"),
        );
    }
    else
    {
        push(@searchNav, "&nbsp;&nbsp;<");
    }
    
    if ($start > 0)
    {
        push(@searchNav, "&nbsp;&nbsp;$start-$end of $max");
    }
        
    if ($nextStart)
    {
        push(@searchNav,
            a({-href=>"javascript:getContentWithVars('index','start=$nextStart&numrows=$numRows')"}, 
                "&nbsp;&nbsp;>"),
        );
    }
    else
    {
        push(@searchNav, "&nbsp;>");
    }

    my $lastStart = $max - $numRows + 1;
    $lastStart = 1 if ($lastStart < 1);
    push(@searchNav,  
        a({-href=>"javascript:getContentWithVars('index','start=$lastStart&numrows=$numRows')"}, "&nbsp;&nbsp;>>"),
    );
    
    push(@pageContent, div({-id=>"resultNav"}, @searchNav));
    
    
    push(@pageContent, br());
    
    
    my @rows;
    push(@rows,
        Tr({-align=>'left', -valign=>'top'},
            th([
                "Index",
                "Name",
                "OP",
                "Tracks",
                "Start",
                "End",
                "Duration",
                "Created",
                "Video",
                "Tape/Live",
                "Descript",
                "Comments",
            ]),
        ),
    );

    if (defined $material)
    {
        my @materialRows;
        my $index = 0;
        foreach my $mp (values %{ $material->{"materials"} })
        {
            my %materialRow;
            my $startTC;
            my $endTC;
            my $duration; 
            
            $materialRow{"name"} = $mp->{"NAME"} || "";
            $materialRow{"op"} = $mp->{"OP"} || "";
            $materialRow{"created"} = $mp->{"CREATION_DATE"} || "";
            

            my @videoTrackNumberRanges;
            my @audioTrackNumberRanges;
            foreach my $track (@{ $mp->{"tracks"} })
            {
                if ($track->{"DATA_DEF_ID"} == 1)
                {
                    merge_track_numbers(\@videoTrackNumberRanges, $track->{'TRACK_NUMBER'});
                }
                else
                {
                    merge_track_numbers(\@audioTrackNumberRanges, $track->{'TRACK_NUMBER'});
                }
                
                my $sourceClip = $track->{"sourceclip"};
                
                #TODO: don't hard code edit rate at 25 fps??
                if (!defined $duration)
                {
                    $duration = $sourceClip->{"LENGTH"} 
                        * (25 / $track->{"EDIT_RATE_NUM"}) * $track->{"EDIT_RATE_DEN"};
                    $materialRow{"duration"} = get_duration_string($duration);
                }
                    
                if ($sourceClip->{"SOURCE_PACKAGE_UID"} !~ /\\0*/)
                {
                    my $sourcePackage = $material->{"packages"}->{
                        $sourceClip->{"SOURCE_PACKAGE_UID"} };
                        
                    if ($sourcePackage 
                        && $sourcePackage->{"descriptor"} # is source package
                        && $sourcePackage->{"descriptor"}->{"TYPE"} == 1) # is file source package
                    {
                        if (!defined $materialRow{"video"})
                        {
                            my $descriptor = $sourcePackage->{"descriptor"};
                            
                            # the video track has the video format
                            if ($descriptor && $descriptor->{"VIDEO_RES"})
                            {
                                $materialRow{"video"} = $descriptor->{"FILE_FORMAT"}.":<br />".$descriptor->{"VIDEO_RES"};
                            }
                        }

                        if (scalar @{ $sourcePackage->{"tracks"} } >= 1) # expecting 1 track in the file package
                        {
                            my $sourceTrack = $sourcePackage->{"tracks"}[0];
                            $sourceClip = $sourceTrack->{"sourceclip"};
                            
                            if (!defined $startTC)
                            {
                                #TODO: don't hard code edit rate at 25 fps??
                                $startTC = $sourceClip->{"POSITION"} 
                                    * (25 / $sourceTrack->{"EDIT_RATE_NUM"}) * $sourceTrack->{"EDIT_RATE_DEN"};
                                $materialRow{"startTC"} = get_timecode_string($startTC);
                            }
                            
                            $sourcePackage = $material->{"packages"}->{
                                $sourceClip->{"SOURCE_PACKAGE_UID"} };
                                
                            if ($sourcePackage 
                                && $sourcePackage->{"descriptor"} # is source package
                                && ($sourcePackage->{"descriptor"}->{"TYPE"} == 2 # is tape source package
                                    || $sourcePackage->{"descriptor"}->{"TYPE"} == 3)) # is live source package
                            {
                                $materialRow{"tapeOrLive"} = $sourcePackage->{"NAME"};
                            }
                        }
                    }
                }
            }
            my $first = 1;
            $materialRow{"tracks"} = "";
            foreach my $range (@videoTrackNumberRanges)
            {
                if ($first)
                {
                    $materialRow{"tracks"} .= "V";
                    $first = 0;
                }
                else
                {
                    $materialRow{"tracks"} .= ",";
                }
                if ($range->[0] != $range->[1])
                {
                    $materialRow{"tracks"} .= "$range->[0]-$range->[1]";
                }
                else
                {
                    $materialRow{"tracks"} .= "$range->[0]";
                }
            }
            $first = 1;
            foreach my $range (@audioTrackNumberRanges)
            {
                if ($first)
                {
                    $materialRow{"tracks"} .= "&nbsp;" if $materialRow{"tracks"};
                    $materialRow{"tracks"} .= "A";
                    $first = 0;
                }
                else
                {
                    $materialRow{"tracks"} .= ",";
                }
                if ($range->[0] != $range->[1])
                {
                    $materialRow{"tracks"} .= "$range->[0]-$range->[1]";
                }
                else
                {
                    $materialRow{"tracks"} .= "$range->[0]";
                }
            }

            $endTC = $startTC + $duration if defined $startTC && defined $duration;
            if ($endTC)
            {
                $materialRow{"endTC"} = get_timecode_string($endTC);
            }
            
            if ($mp->{"usercomments"})
            {
                foreach my $userComment (@{ $mp->{"usercomments"} })
                {
                    if (!defined $materialRow{"descript"})
                    {
                        $materialRow{"descript"} = $userComment->{"VALUE"} if ($userComment->{"NAME"} eq "Descript");
                    }
                    if (!defined $materialRow{"comments"})
                    {
                        $materialRow{"comments"} = $userComment->{"VALUE"} if ($userComment->{"NAME"} eq "Comments");
                    }
                    last if $materialRow{"descript"} && $materialRow{"comments"};
                }
            }

            push(@materialRows, \%materialRow);

            $index++;
        }

        sub materialCmp
        {
            if ($a->{"created"} eq $b->{"created"})
            {
                return $a->{"name"} cmp $b->{"name"}
            }
            else
            {
                return $b->{"created"} cmp $a->{"created"};
            }
        }

        my $prevDate = "";
        my $prevStartTC = "";
        my $bgColour = "grey";
        $index = 0;        
        foreach my $materialRow (sort materialCmp @materialRows)
        {
            my $date = ($materialRow->{"created"} =~ /^(.*)\ .*$/);
            if ($date ne $prevDate || $materialRow->{"startTC"} ne $prevStartTC)
            {
                $bgColour = ($bgColour eq "white") ? "lightgrey" : "white";
                $prevStartTC = $materialRow->{"startTC"};
                $prevDate = $date;
            }
            
            push(@rows,
                Tr({-bgcolor=>$bgColour},
                    td([
                        $index + $start,
                        $materialRow->{"name"},
                        $materialRow->{"op"},
                        $materialRow->{"tracks"} || "",
                        $materialRow->{"startTC"} || "",
                        $materialRow->{"endTC"} || "",
                        $materialRow->{"duration"} || "",
                        #get_local_time($materialRow->{"created"}),
						$materialRow->{"created"},
                        $materialRow->{"video"} || "",
                        $materialRow->{"tapeOrLive"} || "",
                        $materialRow->{"descript"} || "",
                        $materialRow->{"comments"} || "",
                    ]),
                ),
            );
            
            $index++;
        }
    }
    
    push(@pageContent,
        table({-class=>"simpleTable", -id=>"materialTable", -border=>1, -cellspacing=>3,-cellpadding=>3}, @rows)
    );
    
    return join("",@pageContent);
}




END
{
    prodautodb::disconnect($dbh) if ($dbh);
}


