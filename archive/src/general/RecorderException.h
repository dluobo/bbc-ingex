/*
 * $Id: RecorderException.h,v 1.1 2008/07/08 16:23:36 philipn Exp $
 *
 * General purpose exception class
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
 
#ifndef __RECORDER_RECORDER_EXCEPTION_H__
#define __RECORDER_RECORDER_EXCEPTION_H__



#include <string>


#define REC_CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
        throw RecorderException("'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
    }

#define REC_ASSERT(cmd) \
    REC_CHECK(cmd);


    
namespace rec
{
    
class RecorderException
{
public:
    RecorderException();
    RecorderException(const char* format, ...);
    virtual ~RecorderException();
    
    std::string getMessage() const;
    
protected:
    std::string _message;

};




};




#endif


