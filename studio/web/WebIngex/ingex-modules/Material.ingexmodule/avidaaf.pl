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
use File::Temp qw(mktemp);


use lib ".";
use lib "../../ingex-config";
use ingexconfig;
use materialconfig;
use prodautodb;
use db;
use ingexhtmlutil;


my $dbh = prodautodb::connect(
        $ingexConfig{"db_host"},
        $ingexConfig{"db_name"},
        $ingexConfig{"db_user"},
        $ingexConfig{"db_password"}) 
    or die();

my $vresIds = prodautodb::load_video_resolutions($dbh) or 
    return_error_page("failed to load video resolutions from the database: $prodautodb::errstr");

    
my $errorMessage;

    
if (defined param("Cancel1") || defined param("Cancel2"))
{
    redirect_to_page("index.pl");
}
elsif (defined param("Send1") || defined param("Send2"))
{
    my $fromTimeStr;
    my $toTimeStr;
    my $fromDateStr;
    my $toDateStr;
    my $vres;
    my $fromCreationDateStr;
        
    if (defined param("Send1") && !($errorMessage = validate_params($vresIds, "Send1")))
    {
        my $fromDate = time;
        my $toDate = $fromDate;
        if (param("daypop") == 1) # today
        {}
        elsif (param("daypop") == 2) # yesterday
        {
            $fromDate -= 24 * 60 * 60;
            $toDate -= 24 * 60 * 60;
        }
        elsif (param("daypop") == 3) # 2 days ago
        {
            $fromDate -= 2 * 24 * 60 * 60;
            $toDate -= 2 * 24 * 60 * 60;
        }
        else # 3 days ago
        {
            $fromDate -= 3 * 24 * 60 * 60;
            $toDate -= 3 * 24 * 60 * 60;
        }

        
        if (param("periodpop") == 1) # last 10 minutes
        {
            my ($sec,$min,$hour,$mday,$mon,$year) = gmtime(time);
            $min -= 10;
            if ($min < 0)
            {
                $min = 60 + $min;
                $hour -= 1;
                if ($hour < 0)
                {
                    # we don't go back beyond the start of day
                    $hour = 0;
                    $min = 0;
                    $sec = 0;
                }
            }
            $fromCreationDateStr = sprintf("%04d-%02d-%02dT%02d:%02d:%02d", 
                $year + 1900, $mon + 1, $mday, $hour, $min, $sec);
        }
        elsif (param("periodpop") == 2) # last 20 minutes
        {
            my ($sec,$min,$hour,$mday,$mon,$year) = gmtime(time);
            $min -= 20;
            if ($min < 0)
            {
                $min = 60 + $min;
                $hour -= 1;
                if ($hour < 0)
                {
                    # we don't go back beyond the start of day
                    $hour = 0;
                    $min = 0;
                    $sec = 0;
                }
            }
            $fromCreationDateStr = sprintf("%04d-%02d-%02dT%02d:%02d:%02d", 
                $year + 1900, $mon + 1, $mday, $hour, $min, $sec);
        }
        elsif (param("periodpop") == 3) # morning
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d:%02d", 12, 0, 0, 0);    
        }
        elsif (param("periodpop") == 4) # afternoon
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d:%02d", 12, 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);    
            $toDate += 24 * 60 * 60;
        }
        elsif (param("periodpop") == 5) # day
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);   
            $toDate += 24 * 60 * 60;
        }
        elsif (param("periodpop") == 6) # 2 days
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);   
            $toDate += 2 * 24 * 60 * 60;
        }
        else # 3 days
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d:%02d", 0, 0, 0, 0);   
            $toDate += 3 * 24 * 60 * 60;
        }
        
        my ($sec,$min,$hour,$mday,$mon,$year) = localtime($fromDate);
        $fromDateStr = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    

        ($sec,$min,$hour,$mday,$mon,$year) = localtime($toDate);
        $toDateStr = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    
    }
    elsif (defined param("Send2") && !($errorMessage = validate_params($vresIds, "Send2")))
    {
        ($fromDateStr) = param("fromdate") =~ /(\d{4}-\d{1,2}-\d{1,2})/;
        ($fromTimeStr) = param("fromtime") =~ /(\d{1,2}:\d{1,2}:\d{1,2}:\d{1,2})/;
        ($toDateStr) = param("todate") =~ /(\d{4}-\d{1,2}-\d{1,2})/;
        ($toTimeStr) = param("totime") =~ /(\d{1,2}:\d{1,2}:\d{1,2}:\d{1,2})/;
    }
    
    ($vres) = param("vres") =~ /(\d+)/;
    
    if (!$errorMessage)
    {
        my $exportDir = get_avid_aaf_export_dir(param("sendto"));
        my ($filenamePrefix) = param("fprefix") =~ /^\s*(.*\S)\s*$/;
        $filenamePrefix =~ s/\"/\\\"/g;
        
        my $prefix;
        if (!$exportDir || $exportDir =~ /\/$/)
        {
            $prefix = join("", $exportDir, $filenamePrefix);
        }
        else
        {
            $prefix = join("/", $exportDir, $filenamePrefix);
        }

        my $addTSSuffix = (param("ftssuffix") eq "on");

        my $includeDirectorsCut = defined param("directorscut");
        my $dcDb = get_directors_cut_db(param("dcsource"));
        
        
        # TODO: set when installing or hold in central location
        $ENV{"PATH"} = $ingexConfig{"create_aaf_dir"};
        $ENV{"LD_LIBRARY_PATH"} = $ingexConfig{"create_aaf_dir"};

		my $fcpOptions ="";
		if(defined param("software") && param("software") eq "fcp") {
			$fcpOptions = "--xml";
			my $fcploc = param("fcploc");
			if(defined param("fcploc") && $fcploc ne "") {
				$fcpOptions .= " --fcp-path ";
				my ($fcpMaterialDir) = $fcploc =~ /^\s*(.*\S)\s*$/;
				$fcpMaterialDir =~ s/\"/\\\"/g;
				$fcpOptions .= '"'.$fcpMaterialDir.'"';
			}
		}

        # TODO: wait for X seconds and then assume it has failed and then
        # return error to user
        
        #TODO: handle error where no files are included
        
        my $resultsFilename = mktemp("/tmp/createaafresultsXXXXXX");
        my $cmd = join(" ",
                "create_aaf", 
				$fcpOptions,
                "-p \"$prefix\"", # AAF filename prefix
                "-r $vres", # video resolution
                "-o", # group only
                $addTSSuffix ? "" : "--no-ts-suffix",
                "-m", # include multi-camera clips
                $includeDirectorsCut && $dcDb ? "--mc-cuts \"$dcDb\"" : "", # directors cut database
                $fromCreationDateStr ? 
                    "-c $fromCreationDateStr" : # from creation date (timestamp)
                    join(" ", "-f $fromDateStr" . "S" . "$fromTimeStr", # from date and start timecode 
                        $fromCreationDateStr ? "" : "-t $toDateStr" . "S" . "$toTimeStr"), # to date and start timecode
                "-d $ingexConfig{'db_odbc_dsn'}", # database DSN
                "-u $ingexConfig{'db_user'}", # database user
                "--dbpassword $ingexConfig{'db_password'}", # database password
                ">$resultsFilename"
        );
		system($cmd) == 0 or return_error_page("Failed to export Edit file. Using create_aaf in location: $ingexConfig{'create_aaf_dir'} and command $cmd");
        
        # extract the results
        open(AAFRESULTS, "<", "$resultsFilename") 
            or return_error_page("failed to open AAF results file: $!");
        
        my $index = 0;
        my $totalClips;
        my $totalMulticamGroups;
        my $totalDirectorsCutSequences;
        my @filenames;
        my $haveResultsHeader;
        while (my $line = <AAFRESULTS>)
        {
            chomp($line);
            
            if ($haveResultsHeader)
            {
                if ($index == 0)
                {
                    $totalClips = $line;
                }
                elsif ($index == 1)
                {
                    $totalMulticamGroups = $line;
                }
                elsif ($index == 2)
                {
                    $totalDirectorsCutSequences = $line;
                }
                else
                {
                    push(@filenames, $line);
                }
                
                $index++;
            }
            elsif ($line =~ /^RESULTS/)
            {
                $haveResultsHeader = 1;
            }
        }
        
        close(AAFRESULTS);
        unlink($resultsFilename); # be nice and clean up
        
        if (!scalar @filenames)
        {
            return_no_material_page();
        }
        
        
        return_success_page($totalClips, $totalMulticamGroups, $totalDirectorsCutSequences, 
            \@filenames);
    }
}



my $page = get_page_content($vresIds, $errorMessage)
    or return_error_page("failed to fill in content for edit file export page");
       
print header;
print $page;

exit(0);




sub validate_date
{
    my ($date) = @_;
    
    return $date =~ /\d{4}-\d{1,2}-\d{1,2}/;
}

sub validate_time
{
    my ($time) = @_;
    
    return $time =~ /\d{1,2}:\d{1,2}:\d{1,2}:\d{1,2}/;
}

sub validate_params
{
    my ($vresIds, $sendCmd) = @_;
    
    return "Error: missing send-to destination" if (!param("sendto")); 
    return "Error: unknown send-to destination" if (!defined get_avid_aaf_export_dir(param("sendto"))); 
    return "Error: export directory does not exist" 
        unless (-e get_avid_aaf_export_dir(param("sendto")));

    return "Error: missing filename prefix" if (!defined param("fprefix")); 
        
    return "Error: missing video resolution" if (!param("vres")); 
    return "Error: invalid video resolution" if (param("daypop") !~ /\d+/); 
    my $vresId;
    my $found = 0;
    foreach my $vresId (@{$vresIds})
    {
        if ($vresId->{"ID"} eq param("vres"))
        {
            $found = 1;
            last;
        }
    }
    return "Error: unknown video resolution" if (!$found); 
        
    if ($sendCmd eq "Send1")
    {
        return "Error: missing day preset" if (!param("daypop")); 
        return "Error: invalid day preset" if (param("daypop") !~ /[1234]/); 
        return "Error: invalid period preset" if (param("periodpop" !~ /[123]/)); 
    }
    else
    {
        return "Error: missing from date" if (!param("fromdate")); 
        return "Error: invalid from date" if (!validate_date(param("fromdate")));  
        return "Error: missing from time" if (!param("fromtime")); 
        return "Error: invalid from time" if (!validate_time(param("fromtime")));  

        return "Error: missing to date" if (!param("todate")); 
        return "Error: invalid to date" if (!validate_date(param("todate")));  
        return "Error: missing to time" if (!param("totime"));
        return "Error: invalid to time" if (!validate_time(param("totime")));  
    }
    
    return undef;
}
    
sub return_success_page
{
    my ($totalClips, $totalMulticamGroups, $totalDirectorsCutSequences, $filenames) = @_;
    
    my $page = get_success_content($totalClips, $totalMulticamGroups, $totalDirectorsCutSequences, $filenames) or
        return_error_page("failed to fill in content for successful export of aaf file page");
       
    print header;
    print $page;
    
    exit(0);
}

sub return_no_material_page
{
    my $page = get_no_material_content() or
        return_error_page("failed to fill in content for no material exported page");
       
    print header;
    print $page;
    
    exit(0);
}


sub get_success_content
{
    my ($totalClips, $totalMulticamGroups, $totalDirectorsCutSequences, $filenames) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Successfully exported file"));

    push(@pageContent, 
        p(
            span({-class=>"propHeading1"}, "Totals: ") 
        )
    );
    push(@pageContent, 
        table({-border=>0, -cellspacing=>3,-cellpadding=>3},
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    span({-class=>"propHeading2"}, "Clips:"),
                    $totalClips
                ]),
            ),
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    span({-class=>"propHeading2"}, "Multi-camera Clips:"),
                    $totalMulticamGroups
                ]),
            ),
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    span({-class=>"propHeading2"}, "Director's Cut Sequences:"),
                    $totalDirectorsCutSequences
                ]),
            ),
        ),
    );
            
    push(@pageContent, 
        p(
            span({-class=>"propHeading1"}, "Filename: ")
        )
    );
    
    foreach my $filename (@ { $filenames })
    {
        push(@pageContent, "&nbsp;&nbsp;" . $filename); 
        push(@pageContent, "<br/>");
    }
    

    return join("", @pageContent);
}

sub get_no_material_content
{
    my @pageContent;
    
    push(@pageContent, h1("No material to export to edit file"));

    return join("", @pageContent);
}

sub get_page_content
{
    my ($vresIds, $errorMessage) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Edit File Export"));

    
    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    

    push(@pageContent, start_form({-id=>"avidForm", -action=>"javascript:sendForm('avidForm','avidaaf')"}));

    
    my $defaultSendTo;
    my @sendToValues;
    my %sendToLabels;
	my @allExportDirs = get_all_avid_aaf_export_dirs();
    foreach my $exportDir (@allExportDirs)
    {
        if (-e $exportDir->[1])
        {
            my $label = $exportDir->[0] . " ($exportDir->[1])";
            my $value = $exportDir->[0];
            push(@sendToValues, $value);
            $sendToLabels{$value} = $label;
    
            if (!defined $defaultSendTo)
            {
                $defaultSendTo = $value;
            }
        }
    }
    
    if (!scalar @sendToValues)
    {
        return_error_page("None of the send-to directories exist - please check the web config file");
    }
    
    push(@pageContent,
        p(
            "Export for:",
            popup_menu({
                -name => "software", 
                -default => 'avid',
                -values => ['avid','fcp'],
                -labels => {'avid'=>'Avid','fcp'=>'Final Cut Pro'},
            }),
        ),
    );

	push(@pageContent,
        p(
            "Send to:",
            popup_menu({
                -name => "sendto", 
                -default => $defaultSendTo,
                -values => \@sendToValues,
                -labels => \%sendToLabels,
            }),
        ),
    );

    push(@pageContent,
        p(
            "Filename Prefix:",
            textfield({
                name => "fprefix", 
                value => $ingexConfig{"avid_aaf_prefix"},
                size => 16
            }),
            checkbox({
                name => 'ftssuffix',
                checked => 1,
                value => 'on',
                label => 'Add Long Suffix'
            }),
        ),
    );

	push(@pageContent,
        p(
            "Editing directory path:",
            textfield({
                name => "fcploc", 
                value => "",
                size => 16
            }),
			"(Final Cut Pro only)"
        ),
    );
    
    
    my @vresValues;
    my %vresLabels;
    foreach my $vresId (@{$vresIds})
    {
        my $label = $vresId->{"NAME"};
        my $value = $vresId->{"ID"};
        push(@vresValues, $value);
        $vresLabels{$value} = $label;
    }
    push(@pageContent,
        p(
            "Video resolution:",
            popup_menu({
                -name => "vres", 
                -default => 8,
                -values => \@vresValues,
                -labels => \%vresLabels,
            }),
        ),
    );

    push(@pageContent,
        p(
            checkbox({
                name => 'directorscut',
                checked => 1,
                value => 'on',
                label => 'Add Director\'s Cut'
            }),
        ),
    );

    my $defaultDCSource;
    my @dcSourceValues;
    my %dcSourceLabels;
    foreach my $dctDb (get_all_directors_cut_dbs())
    {
        if (-e $dctDb->[1])
        {
            my $label = $dctDb->[0] . " ($dctDb->[1])";
            my $value = $dctDb->[0];
            push(@dcSourceValues, $value);
            $dcSourceLabels{$value} = $label;
    
            if (!defined $defaultDCSource)
            {
                $defaultDCSource = $value;
            }
        }
    }
    
    push(@pageContent,
        p(
            "Director's Cut Source:",
            popup_menu({
                -name => "dcsource", 
                -default => $defaultDCSource,
                -values => \@dcSourceValues,
                -labels => \%dcSourceLabels,
            }),
        ),
    );
    
    
    push(@pageContent, h3("Select preset time period")); 

    push(@pageContent, 
        table({-border=>0, -cellspacing=>3,-cellpadding=>3},
            Tr({-align=>"left", -valign=>"top"}, 
                th(["Day", "Period"]),
            ),
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    popup_menu(
                        -name => "daypop", 
                        -default => 1,
                        -values => [1, 2, 3, 4],
                        -labels => {
                            1 => "today",
                            2 => "yesterday",
                            3 => "2 days ago",
                            4 => "3 days ago",
                        }
                    ), 
                    popup_menu(
                        -name => "periodpop", 
                        -default => 1,
                        -values => [1, 2, 3, 4, 5, 6],
                        -labels => {
                            1 => "last 10 minutes",
                            2 => "last 20 minutes",
                            3 => "morning (00:00 - 11:59)",
                            4 => "afternoon (12:00 - 23:59)",
                            5 => "day",
                            6 => "2 days",
                            7 => "3 days",
                        }
                    ), 
                ]),
            ),
        ),
    );

    push(@pageContent, submit({-name=>"Send1", -label=>"Send", -onclick=>"whichPressed=this.name"}));


    my ($sec,$min,$hour,$mday,$mon,$year) = localtime(time);
    my $nowDate = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    
    my $nowTime = sprintf("%02d:%02d:%02d:%02d", $hour, $min, $sec, 0);    

    
    push(@pageContent, h3("Select precise time period")); 
    
    push(@pageContent, 
        table({-border=>0, -cellspacing=>3,-cellpadding=>3},
            Tr({-align=>"left", -valign=>"top"}, 
                th(["", "Date <small><i>yyyy-mm-dd</i></small>", 
                    "Start timecode <small><i>hh:mm:ss::ff</i></small>"
                ]),
            ),
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    "From",
                    textfield({
                        name => "fromdate", 
                        value => $nowDate,
                        size => 10,
                        maxlength => 10
                    }),
                    textfield({
                        name => "fromtime", 
                        value => "00:00:00:00",
                        size => 11,
                        maxlength => 11
                    }),
                ]),
            ),
            Tr({-align=>"left", -valign=>"top"}, 
                td([
                    "To",
                    textfield({
                        name => "todate", 
                        value => $nowDate,
                        size => 10,
                        maxlength => 10
                    }),
                    textfield({
                        name => "totime", 
                        value => $nowTime,
                        size => 11,
                        maxlength => 11
                    }),
                ]),
            ),
        ),
    );

    push(@pageContent, submit({-name=>"Send2", -label=>"Send", -onclick=>"whichPressed=this.name"}));


    push(@pageContent, end_form);

    
    return join("",@pageContent);
}


END
{
    prodautodb::disconnect($dbh) if ($dbh);
}

