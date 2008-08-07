/*
 * $Id: ContentProcessor.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
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

#include <CorbaUtil.h>
#include "ContentProcessor.h"

ContentProcessor::ReturnCode ContentProcessor::ResolveDevice()
{
    ReturnCode result = OK;
    if(mName.empty())
    {
        result = NO_DEVICE;
    }

    CosNaming::Name name;
    name.length(3); 
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("Processors");
    name[2].id = CORBA::string_dup(mName.c_str());

    CORBA::Object_var obj;
    if(OK == result)
    {
        obj = CorbaUtil::Instance()->ResolveObject(name);
    }

    if(CORBA::is_nil(obj))
    {
        result = NO_DEVICE;
    }

    if(OK == result)
    {
        try
        {
            //mRef = ProdAuto::Processor::_narrow(obj);
            mRef = ProdAuto::Subject::_narrow(obj);
        }
        catch(const CORBA::Exception &)
        {
            result = NO_DEVICE;
        }
    }

    return result;
}


ContentProcessor::ReturnCode ContentProcessor::Process(const char * args)
{
    bool ok_so_far = true;
    ReturnCode return_code = OK;

    if(CORBA::is_nil(mRef))
    {
        ok_so_far = false;
        return_code = NO_DEVICE;
    }

    if(ok_so_far)
    {
        try
        {
            //mRef->Process(args);
            mRef->Notify(args);
        }
        catch (const CORBA::Exception &)
        {
            ok_so_far = false;
            return_code = FAILED;
        }
    }

    return return_code;
}
