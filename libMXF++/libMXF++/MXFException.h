/*
 * $Id: MXFException.h,v 1.2 2009/10/12 15:30:25 philipn Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __MXFPP_MXFEXCEPTION_H__
#define __MXFPP_MXFEXCEPTION_H__


#include <string>
#include <cassert>


namespace mxfpp
{


    
#define MXFPP_CHECK(cmd) \
    if (!(cmd)) \
    { \
        mxf_log(MXF_ELOG, "'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
        throw MXFException("'%s' failed, at %s:%d\n", #cmd, __FILE__, __LINE__); \
    }

#if defined(NDEBUG)
#define MXFPP_ASSERT(cmd) \
    MXFPP_CHECK(cmd);
#else
#define MXFPP_ASSERT(cond) \
    assert(cond)
#endif


class MXFException
{
public:
    MXFException();
    MXFException(const char* format, ...);
    virtual ~MXFException();
    
    std::string getMessage() const;
    
protected:
    std::string _message;

};


};




#endif

