/*
 * $Id: indexfile.cpp,v 1.1 2008/07/08 16:26:42 philipn Exp $
 *
 * Boilerplate text added to the LTO index file
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "indexfile.h"

string boiler_plate =
"\n"
"V1.0\n"
"Tape Access\n"
"\n"
"Each file is written to tape using the TAR (TapeARchive) standard.  (POSIX.1-2001 TAR http://www.unix.org/version3/ ).\n"
"There is no spanning of tapes. Only complete files are stored on a single tape.\n"
"There is no tape level compression.  This mitigates data loss in the rare event of media error.\n"
"A data tape block size of 256KiB is used.  This was chosen to optimise LTO-3 data transfer speeds.\n"
"The LTO tape is laid out as a series of files separated by LTO file-markers.  \n"
"\n"
"File Arrangement\n"
"\n"
"The first file on the tape is the so-called 'index file' for the tape.  (This is described below).  Subsequent files are those which have been archived onto the data tape.\n"
"\n"
"The file arrangement does not allow for additional files to be added to the tape in a later session.  (Since it is not possible to append the index file with details of later additions to the tape).\n"
"\n"
"Filenaming\n"
"\n"
"The filename of the index file is <LTO_spool_no>00.txt  e.g. LTA62749100.txt where the LTO_spool_no is LTA627491.\n"
"\n"
"Subsequent filenames of the files held on the tape will either be those of the original files transferred to LTO, or filenames constructed according to the details contained in Appendix A - (re-badged files). e.g. LTA62749101.mxf, LTA62749102.mxf etc. \n"
"The form of filename used for the index file is fixed, irrespective of whether the other files stored on the tape have been 're-badged'.  \n"
"\n"
"Index file\n"
"\n"
"The first file of the LTO tape is a so-called index file.  This is a human and machine readable text file containing:\n"
"\n"
"(i) the bar code/spool no. of the tape,\n"
"(ii) a directory listing of the tape,\n"
"(iii) 'Boiler-plate text' describing the file arrangement format of the tape, and of the index file itself.\n"
"\n"
"The filename of the index file is <LTO_spool_no>00.txt  e.g. LTA62749100.txt\n"
"\n"
"(i) The first line of the index file contains only the bar code/spool no. of the LTO tape that the index file refers to.\n"
"\n"
"(ii) The second, and subsequent similar lines (the directory listing), contain information relating to the files contained on the tape - listed in the order that they appear on the tape.  Each line consists of:\n"
"\n"
"a) the numerical position of the file on the tape - the position of the index file being 00, (this field has a minimum width of two-digits).\n"
"b) the filename of the file,\n"
"c) the file size in bytes (a fixed-length field of 13 digits, padded with leading zeros where necessary).\n"
"\n"
"The items are separated by single TAB characters.\n"
"\n"
"The first line of the directory listing (second line of the index file) contains information relating to the index file itself.\n"
"\n"
"The list is terminated by an empty line.\n"
"\n"
"(iii) A section of text (the 'boiler-plate text') follows the directory list terminator.  This text contains information describing the file arrangement format of the tape, and of the index file, and the version number of the index file format.\n"
"\n"
"Appendix A - File re-badging\n"
"\n"
"With file re-badging the files transferred to LTO are given new names which are derived from the LTO_spool_number together with the order in which the files have been laid to tape.  The original filename-extensions are retained.\n"
"\n"
"The format of the re-badged files is :\n"
"\n"
"<LTO_spool_number><filename_position>.<original_filename-extension>\n"
"\n"
"The field-length of the filename_position field is two-digits, therefore there is a limit to the number of files which can be re-badged for storage onto LTO tape of 99 files.  The index file always has a filename_position of 00 (zero).\n"
"\n"
"\n"
"e.g.\n"
"\n"
"storeLTO -r DA_03196301.mxf+DC_11884101.mxf+DC_16792702.mxf+DC_10604701.mxf+DC_10327101.mxf+DA_01764001.mxf+DA_01764003.mxf+DA_01764004.mxf+DA_01764005.mxf+DC_09210701.mxf LTA001482\n"
"\n"
"If the above command was issued using the storeLTO software application, the files would be re-badged as follows on the LTO tape.\n"
"\n"
"index.txt  ->  LTA00148200.txt\n"
"DA_03196301.mxf    ->  LTA00148201.mxf\n"
"DC_11884101.mxf    ->  LTA00148202.mxf\n"
"DC_16792702.mxf    ->  LTA00148203.mxf\n"
"DC_10604701.mxf    ->  LTA00148204.mxf\n"
"DC_10327101.mxf    ->  LTA00148205.mxf\n"
"DA_01764001.mxf    ->  LTA00148206.mxf\n"
"DA_01764003.mxf    ->  LTA00148207.mxf\n"
"DA_01764004.mxf    ->  LTA00148208.mxf\n"
"DA_01764005.mxf    ->  LTA00148209.mxf\n"
"DC_09210701.mxf    ->  LTA00148210.mxf\n"
"\n"
"\n"
"\n"
"James Insell   james.insell@bbc.co.uk  10/10/2007\n"
"\n";


void lto_rebadge_name(const char *lto_id, unsigned file_num, string *p)
{
    char newname[FILENAME_MAX];
    sprintf(newname, "%s%02u.mxf", lto_id, file_num);
    *p = newname;
}

