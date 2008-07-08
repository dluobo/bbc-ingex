/*
 * $Id: Utilities.h,v 1.1 2008/07/08 16:23:46 philipn Exp $
 *
 * Common utility functions
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
 
#ifndef __RECORDER_UTILITIES_H__
#define __RECORDER_UTILITIES_H__


#include <sys/time.h>
#include <string>
#include <vector>

#include "Types.h"
#include "Logging.h"


#define SAFE_DELETE(var) \
    delete var; \
    var = 0;

#define SAFE_ARRAY_DELETE(var) \
    delete [] var; \
    var = 0;


    
namespace rec
{

std::string join_path(std::string path1, std::string path2); 
std::string join_path(std::string path1, std::string path2, std::string path3); 
std::string strip_path(std::string filename); 
std::string strip_name(std::string filename); 

std::string get_host_name();

std::string get_user_timestamp_string(Timestamp value);

std::string int_to_string(int value);
std::string long_to_string(long value);
std::string int64_to_string(int64_t value);

Date get_todays_date();

Timestamp get_localtime_now();


void clear_old_log_files(std::string directory, std::string prefix, std::string suffix, int daysOld);
std::string add_timestamp_to_filename(std::string prefix, std::string suffix);


bool directory_exists(std::string path);
bool directory_path_is_absolute(std::string path);
bool file_exists(std::string filename);

bool parse_log_level_string(std::string level, LogLevel* level);

std::string trim_string(std::string val);


bool is_valid_barcode(std::string barcode);


template <class Element>
class AutoPointerVector
{
public:
    typedef typename std::vector<Element*> VecType;
    typedef typename VecType::const_iterator VecTypeConstIter;

    AutoPointerVector() {}
    AutoPointerVector(const VecType& vec)
    {
        _vec = vec;
    }

    ~AutoPointerVector()
    {
        VecTypeConstIter iter;
        for (iter = _vec.begin(); iter != _vec.end(); iter++)
        {
            delete *iter;
        }
    }

    VecType& get()
    {
        return _vec;
    }

    VecType release()
    {
        VecType ret = _vec;
        _vec.clear();

        return ret;
    }

private:
    VecType _vec;
};


};


#endif


