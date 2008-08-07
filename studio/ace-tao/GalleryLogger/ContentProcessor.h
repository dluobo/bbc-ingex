/*
 * $Id: ContentProcessor.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to manage communication with a content processor.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#ifndef ContentProcessor_h
#define ContentProcessor_h

#include <string>

#include "tao/ORB.h"
//#include "processorC.h"
#include "SubjectC.h"

/**
Interface to a remote content processor.
The underlying interface uses Corba.
*/
class ContentProcessor
{
public:
    enum ReturnCode { OK, NO_DEVICE, FAILED };

    void SetDeviceName(const char * name) {mName = name;}
    ReturnCode ResolveDevice();
    ReturnCode Process(const char * args);

private:
    std::string mName;
    //ProdAuto::Processor_var mRef;
    ProdAuto::Subject_var mRef;
};

#endif //#ifndef ContentProcessor_h
