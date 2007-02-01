#!/usr/bin/perl -wT

#
# $Id: avidaaf.pl,v 1.1 2007/02/01 09:02:47 philipn Exp $
#
# 
#
# Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#
 
use strict;

use CGI::Pretty qw(:standard);
use File::Temp qw(mktemp);


use lib ".";
use config;
use htmlutil;



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
        
    if (defined param("Send1") && !($errorMessage = validate_params("Send1")))
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

        
        if (param("periodpop") == 1) # morning
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d", 12, 0, 0);    
        }
        elsif (param("periodpop") == 2) # afternoon
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d", 12, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);    
            $toDate += 24 * 60 * 60;
        }
        elsif (param("periodpop") == 3) # day
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);   
            $toDate += 24 * 60 * 60;
        }
        elsif (param("periodpop") == 4) # 2 days
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);   
            $toDate += 2 * 24 * 60 * 60;
        }
        else # 3 days
        {
            $fromTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);    
            $toTimeStr = sprintf("%02d:%02d:%02d", 0, 0, 0);   
            $toDate += 3 * 24 * 60 * 60;
        }
        
        my ($sec,$min,$hour,$mday,$mon,$year) = localtime($fromDate);
        $fromDateStr = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    

        ($sec,$min,$hour,$mday,$mon,$year) = localtime($toDate);
        $toDateStr = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    
    }
    elsif (defined param("Send2") && !($errorMessage = validate_params("Send2")))
    {
        ($fromDateStr) = param("fromdate") =~ /(\d{4}-\d{1,2}-\d{1,2})/;
        ($fromTimeStr) = param("fromtime") =~ /(\d{1,2}:\d{1,2}:\d{1,2})/;
        ($toDateStr) = param("todate") =~ /(\d{4}-\d{1,2}-\d{1,2})/;
        ($toTimeStr) = param("totime") =~ /(\d{1,2}:\d{1,2}:\d{1,2})/;
    }
    
    if (!$errorMessage)
    {
        my $exportDir = get_avid_aaf_export_dir(param("sendto"));
        
        $ingexConfig{"avid_aaf_prefix"} =~ /(\S*)/;
        my $prefix;
        if (!$exportDir || $exportDir =~ /\/$/)
        {
            $prefix = join("", $exportDir, $1);
        }
        else
        {
            $prefix = join("/", $exportDir, $1);
        }
        
        # TODO: set when installing or hold in central location
        $ENV{"PATH"} = $ingexConfig{"create_aaf_dir"};
        $ENV{"LD_LIBRARY_PATH"} = $ingexConfig{"create_aaf_dir"};

        # TODO: wait for X seconds and then assume it has failed and then
        # return error to user
        
        #TODO: handle error where no files are included
        
        my $filenamesFilename = mktemp("/tmp/aaffilenamesXXXXXX");
        system(join(" ",
                "create_aaf", 
                "-p $prefix", # AAF filename prefix
                "-o", # group only
                "-m", # include multi-camera clips
                "-f $fromDateStr" . "L" . "$fromTimeStr", # from timestamp 
                "-t $toDateStr" . "L" . "$toTimeStr", # to timestamp
                "-d $ingexConfig{'db_odbc_dsn'}", # database DSN
                "-u $ingexConfig{'db_user'}", # database user
                "--dbpassword $ingexConfig{'db_password'}", # database password
                ">$filenamesFilename"
        )) == 0 or return_error_page("failed to export Avid AAF file");
        
        # extract the list of filenames
        open(AAFFILENAMES, "<", "$filenamesFilename") 
            or return_error_page("failed to open AAF filename list: $!");
        
        my @filenames;
        while (my $filename = <AAFFILENAMES>)
        {
            push(@filenames, $filename);
        }
        
        close(AAFFILENAMES);
        unlink($filenamesFilename); # be nice and clean up
        
        if (!scalar @filenames)
        {
            return_no_material_page();
        }
        
        
        return_success_page("Created AAF file: ". join(",", @filenames));
    }
}



my $page = construct_page(get_page_content($errorMessage)) 
    or return_error_page("failed to fill in content for Avid AAF export page");
       
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
    
    return $time =~ /\d{1,2}:\d{1,2}:\d{1,2}/;
}

sub validate_params
{
    my ($sendCmd) = @_;
    
    return "Error: missing send-to destination" if (!param("sendto")); 
    return "Error: unknown send-to destination" if (!defined get_avid_aaf_export_dir(param("sendto"))); 
    return "Error: export directory does not exist" 
        unless (-e get_avid_aaf_export_dir(param("sendto")));
    
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
    my ($message) = @_;
    
    my $page = construct_page(get_success_content($message)) or
        return_error_page("failed to fill in content for successful export of aaf file page");
       
    print header;
    print $page;
    
    exit(0);
}

sub return_no_material_page
{
    my $page = construct_page(get_no_material_content()) or
        return_error_page("failed to fill in content for no material exported page");
       
    print header;
    print $page;
    
    exit(0);
}


sub get_success_content
{
    my ($message) = @_;
    
    
    my @pageContent;
    
    push(@pageContent, h1("Successfully exported Avid AAF file"));

    push(@pageContent, p($message));

    return join("", @pageContent);
}

sub get_no_material_content
{
    my @pageContent;
    
    push(@pageContent, h1("No material to export to Avid AAF file"));

    return join("", @pageContent);
}

sub get_page_content
{
    my ($errorMessage) = @_;
    
    my @pageContent;
    
    push(@pageContent, h1("Avid AAF file export"));

    
    if (defined $errorMessage)
    {
        push(@pageContent, p({-class=>"error"}, $errorMessage));
    }
    

    push(@pageContent, start_form({-method=>"POST", -action=>"avidaaf.pl"}));

    
    my $defaultSendTo;
    my @sendToValues;
    my %sendToLabels;
    foreach my $exportDir (get_all_avid_aaf_export_dirs())
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
            "Send to:",
            popup_menu({
                -name => "sendto", 
                -default => $defaultSendTo,
                -values => \@sendToValues,
                -labels => \%sendToLabels,
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
                        -values => [1, 2, 3, 4, 5],
                        -labels => {
                            1 => "morning (00:00 - 11:59)",
                            2 => "afternoon (12:00 - 23:59)",
                            3 => "day",
                            4 => "2 days",
                            5 => "3 days",
                        }
                    ), 
                ]),
            ),
        ),
    );

    push(@pageContent, submit({-name=>"Send1", -label=>"Send"}));


    my ($sec,$min,$hour,$mday,$mon,$year) = localtime(time);
    my $nowDate = sprintf("%04d-%02d-%02d", $year + 1900, $mon + 1, $mday);    
    my $nowTime = sprintf("%02d:%02d:%02d", $hour, $min, $sec);    

    
    push(@pageContent, h3("Select precise time period")); 
    
    push(@pageContent, 
        table({-border=>0, -cellspacing=>3,-cellpadding=>3},
            Tr({-align=>"left", -valign=>"top"}, 
                th(["", "Date <small><i>yyyy-mm-dd</i></small>", 
                    "Time <small><i>hh:mm:ss</i></small>"
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
                        value => "00:00:00",
                        size => 8,
                        maxlength => 8
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
                        size => 8,
                        maxlength => 8
                    }),
                ]),
            ),
        ),
    );

    push(@pageContent, submit({-name=>"Send2", -label=>"Send"}));


    push(@pageContent, end_form);

    
    return join("",@pageContent);
}


