/*
 * $Id: FileUtils.cpp,v 1.1 2006/12/20 12:28:29 john_f Exp $
 *
 * File utilities.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "FileUtils.h"

#include <ace/OS_NS_sys_stat.h>
#include <ace/OS_NS_string.h>
#include <ace/OS_NS_errno.h>

#include <vector>
#include <iostream>

static const char * delim_set = "\\/";
#if defined(WIN32)
static const char * delim = "\\";
#else
static const char * delim = "/";
#endif

const mode_t mask = 002; // mask only o:w
const mode_t dir_mode = 02775; // srwxrwxr-x

/**
This function checks that the directory exists and, if it does not, the
directory is created (including any parent directories that did not exist).
*/
bool FileUtils::CreatePath(const std::string & path)
{
    // Debug
    //std::cerr << "FileUtils::CreatePath(" << path << ")" << std::endl;

    ACE_OS::umask(mask);

    // Make copy of path for use by strtok
    char tmp_path[255];
    ACE_OS::strcpy(tmp_path, path.c_str());

    // Check for path beginning at root
    bool root_path = (*delim == tmp_path[0]);

    // Find each path component and store
    std::vector<std::string> path_elements;
    const char * path_element;
    char * str = tmp_path;
    //std::cerr << "Path components:\n";
    while(NULL != (path_element = ACE_OS::strtok(str, delim_set)))
    {
        str = NULL;  // for subsequent calls to strtok
        path_elements.push_back(path_element);
        //std::cerr << path_element << std::endl;
    }

    //debug
    //for(int i = 0; i < path_elements.size(); ++i)
    //{
    //    std::cerr << path_elements[i] << "\n";
    //}

    // Now build up the path a component at a time, checking that each
    // directory exists and creating if necessary.
    ACE_stat tmpstat;
    std::string trial_path;
    if(root_path)
    {
        trial_path = delim;
    }
    int n_elements = path_elements.size();
    //debug
    //for(int j = 0; j < n_elements; ++j)
    //{
    //    std::cerr << path_elements[j] << " ";
    //}
    bool result = true;
    for(int i = 0; i < n_elements && result; ++i)
    {
        //debug
        //for(int j = 0; j < n_elements; ++j)
        //{
        //    std::cerr << path_elements[j] << " ";
        //}
        //std::cerr << std::endl;
        //std::cerr << "Element " << i << ": " << path_elements[i] << "\n";
        trial_path += path_elements[i];
        //std::cerr << "Trial path: " << trial_path << std::endl;
        if(0 == ACE_OS::stat(trial_path.c_str(), &tmpstat))
        {
            // directory exists
        }
        else if(ENOENT == ACE_OS::last_error())
        {
            // creating directory
            std::cerr << "Creating: " << trial_path << std::endl;
            result = (0 == ACE_OS::mkdir(trial_path.c_str(), dir_mode));
        }
        else
        {
            // stat failed
            result = false;
        }
        trial_path += delim;
    }

    return result;
}


/**
This function checks that the directory given does not already exist
and then creates it (including any parent directories that did not exist).
If the directory does already exist, the name is modified to one that does not.
Input path should not end with slash.
*/
bool FileUtils::CreateUniquePath(std::string & path)
{
    // Debug
    //std::cerr << "FileUtils::CreateUniquePath(" << path << ")" << std::endl;

    ACE_OS::umask(mask);

    // Make copy of path for use by strtok
    char tmp_path[255];
    ACE_OS::strcpy(tmp_path, path.c_str());

    // Check for path beginning at root
    bool root_path = (*delim == tmp_path[0]);

    // Find each path component and store
    std::vector<std::string> path_elements;
    const char * path_element;
    char * str = tmp_path;
    std::cerr << "Path components:\n";
    while(NULL != (path_element = ACE_OS::strtok(str, delim_set)))
    {
        str = NULL;  // for subsequent calls to strtok
        path_elements.push_back(path_element);
        std::cerr << path_element << std::endl;
    }

    //debug
    //for(int i = 0; i < path_elements.size(); ++i)
    //{
    //    std::cerr << path_elements[i] << "\n";
    //}

    // Now build up the path a component at a time, checking that each
    // directory exists and creating if necessary but stopping short of
    // final directory.
    ACE_stat tmpstat;
    std::string trial_path;
    if(root_path)
    {
        trial_path = delim;
    }
    int n_elements = path_elements.size();
    //debug
    //for(int j = 0; j < n_elements; ++j)
    //{
    //    std::cerr << path_elements[j] << " ";
    //}
    for(int i = 0; i < n_elements - 1; ++i)
    {
        //debug
        //for(int j = 0; j < n_elements; ++j)
        //{
        //    std::cerr << path_elements[j] << " ";
        //}
        //std::cerr << std::endl;
        //std::cerr << "Element: " << path_elements[i] << "\n";
        trial_path += path_elements[i];
        //std::cerr << "Trial path: " << trial_path << std::endl;
        if(0 == ACE_OS::stat(trial_path.c_str(), &tmpstat))
        {
            // directory exists
        }
        else if(ENOENT == ACE_OS::last_error())
        {
            // creating directory
            std::cerr << "Creating: " << trial_path << std::endl;
            ACE_OS::mkdir(trial_path.c_str(), dir_mode);
        }
        else
        {
            // stat failed
        }
        trial_path += delim;
    }

    // Now check final directory to ensure it doesn't already exist.
    trial_path += path_elements.back();
    for(int count = 0; count < 10 && 0 == ACE_OS::stat(trial_path.c_str(), &tmpstat); ++count)
    {
        // directory exists so we modify name and try again
        trial_path += '_';
    }
    // Create the final directory
    bool result = false;
    if(ENOENT == ACE_OS::last_error())
    {
        // creating directory
        std::cerr << "Creating: " << trial_path << std::endl;
        result = (0 == ACE_OS::mkdir(trial_path.c_str(), dir_mode));
    }

    // Return the path if successful
    if(result)
    {
        path = trial_path;
        // Debug
        std::cerr << "FileUtils::CreateUniquePath() created\"" << path << "\"" << std::endl;
    }
    else
    {
        // Debug
        std::cerr << "FileUtils::CreateUniquePath() failed to create \"" << trial_path << "\"" << std::endl;
    }

    return result;
}
