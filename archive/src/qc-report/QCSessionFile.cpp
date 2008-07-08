/*
 * $Id: QCSessionFile.cpp,v 1.1 2008/07/08 16:25:19 philipn Exp $
 *
 * Provides access to information contained in a qc_player session file
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#include "QCSessionFile.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;


static string trim_string(string val)
{
    size_t start;
    size_t len;
    
    // trim spaces from the start
    start = 0;
    while (start < val.size() && isspace(val[start]))
    {
        start++;
    }
    if (start >= val.size())
    {
        // empty string or string with spaces only
        return "";
    }
    
    // trim spaces from the end by reducing the length
    len = val.size() - start;
    while (len > 0 && isspace(val[start + len - 1]))
    {
        len--;
    }
    
    return val.substr(start, len);
}

static string parse_quoted_string(string quotedStr)
{
    size_t start;
    size_t len;
    char quoteChar;
    
    // find first quote char
    start = 0;
    while (start < quotedStr.size() && quotedStr[start] != '\'' && quotedStr[start] != '\"')
    {
        start++;
    }
    if (start >= quotedStr.size())
    {
        // no quote char found - empty value
        return "";
    }
    quoteChar = quotedStr[start];
    start++;
    
    // find next quote char
    len = 0;
    while (start + len < quotedStr.size() && quotedStr[start + len] != quoteChar)
    {
        len++;
    }
    if (start + len >= quotedStr.size())
    {
        // invalid quoted string - empty value
        return "";
    }
    
    return quotedStr.substr(start, len);
}

static bool read_next_line(FILE* file, vector<string>* line)
{
    *line = vector<string>();
    
    int c;
    while (true)
    {
        c = fgetc(file);
        if (c != '\n' && '\r')
        {
            break;
        }
    }
    if (c == EOF)
    {
        return false;
    }

    string field;
    field.reserve(64);
    do
    {
        if (c == '\n' || c == '\r' || c == EOF)
        {
            if (!line->empty() || !field.empty())
            {
                line->push_back(trim_string(field));
            }
            break;
        }
        else if (c == ',')
        {
            line->push_back(trim_string(field));
            field = "";
        }
        else
        {
            field.append(1, (char)c);
        }

        c = fgetc(file);
    }
    while (true);
    
    return true;
}

static int64_t parse_int64(string field)
{
    int64_t result;
    
    if (sscanf(field.c_str(), "%"PRId64"", &result) != 1)
    {
        throw RecorderException("Failed to parse mark position '%s' from session file", field.c_str());
    }
    
    return result;
}

static int parse_type(string field)
{
    int result;
    
    if (sscanf(field.c_str(), "0x%x", &result) != 1)
    {
        throw RecorderException("Failed to parse mark type '%s' from session file", field.c_str());
    }
    
    return result;
}

static Mark parse_mark(vector<string>& line)
{
    Mark result;
    
    result.position = parse_int64(line[1]);
    result.type = parse_type(line[5]);
    if (!trim_string(line[3]).empty())
    {
        result.duration = parse_int64(line[3]);
    }
    
    return result;
}



QCSessionFile::QCSessionFile(string filename)
: _filename(filename), _requireRetransfer(false)
{
    FILE* file = fopen(filename.c_str(), "rb");
    if (file == 0)
    {
        throw RecorderException("Failed to open qc session file '%s': %s", filename.c_str(), strerror(errno));
    }
    
    // comments are limited to 400 characters so reserve that much space
    _comments.reserve(400);
    
    vector<string> line;
    int state = 0;
    bool haveFirstMark = false;
    bool done = false;
    Mark mark;
    try
    {
        while (!done && read_next_line(file, &line))
        {
            switch (state)
            {
                // get loaded session filename, operator and station, and find start of marks
                case 0:
                    if (!line.empty())
                    {
                        if (line[0].compare(0, strlen("# Loaded session"), "# Loaded session") == 0)
                        {
                            _loadedSessionFilename = parse_quoted_string(&line[0][strlen("# Loaded session")]);
                        }
                        else if (line[0].compare(0, strlen("# Username"), "# Username") == 0)
                        {
                            _operator = parse_quoted_string(&line[0][strlen("# Username")]);
                        }
                        else if (line[0].compare(0, strlen("# Hostname"), "# Hostname") == 0)
                        {
                            _station = parse_quoted_string(&line[0][strlen("# Hostname")]);
                        }
                        else if (line[0].compare(0, strlen("# Marks"), "# Marks") == 0)
                        {
                            state = 1;
                        }
                        else if (line[0].compare(0, strlen("# Comments"), "# Comments") == 0)
                        {
                            state = 2;
                        }
                    }
                    break;

                // read marks
                case 1:
                    if (line[0].compare(0, strlen("# Comments"), "# Comments") == 0)
                    {
                        state = 2;
                        break;
                    }
                    if (line[0].compare(0, strlen("#"), "#") == 0)
                    {
                        if (!haveFirstMark)
                        {
                            // skip initial comment lines
                            break;
                        }
                        else 
                        {
                            // end of marks
                            state = 0;
                            break;
                        }
                    }
                    
                    mark = parse_mark(line);
                    haveFirstMark = true;
                    
                    // filter out PSE failure and D3 error marks if present
                    mark.type &= ~(D3_PSE_FAILURE_MARK_TYPE | D3_VTR_ERROR_MARK_TYPE);
                    if (mark.type == 0)
                    {
                        break;
                    }
                    
                    _userMarks.push_back(mark);
                    
                    // extract programme clip
                    if (mark.duration != 0 && _programmeClip.position < 0)
                    {
                        _programmeClip = mark;
                    }
                    
                    // extract transfer marker
                    if (mark.type & TRANSFER_MARK_TYPE)
                    {
                        _requireRetransfer = true;
                    }
                    
                    break;

                // read user comments
                case 2:
                    
                    // skip (ignore) the line just read and read the comments
                    int c;
                    while ((c = fgetc(file)) != EOF)
                    {
                        _comments.append(1, (char)c);
                    }
                    _comments = trim_string(_comments);
                    
                    // we are done because comments are always at the end
                    done = 1;
                    break;
            }
                    
        }
        
        
        fclose(file);
    }
    catch (...)
    {
        fclose(file);
        throw;
    }
}

QCSessionFile::~QCSessionFile()
{
}
    

