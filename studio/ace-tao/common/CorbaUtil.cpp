/*
 * $Id: CorbaUtil.cpp,v 1.2 2008/09/03 13:43:33 john_f Exp $
 *
 * CORBA utilities
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

#include <ace/Log_Msg.h>

#include <tao/corba.h>
#include <tao/PortableServer/PortableServer.h>
#include <tao/Stub.h>       // needed for decoding mutliple profile iors
#include <tao/Profile.h>    // needed for decoding mutliple profile iors
#include <tao/TimeBaseC.h>
#include <tao/Messaging/Messaging.h>

#include <sstream>

#include "CorbaUtil.h"


// static member
CorbaUtil * CorbaUtil::mInstance = 0;

// constructor
CorbaUtil::CorbaUtil()
{
// orb initially nil
    mOrb = CORBA::ORB::_nil();
}

void CorbaUtil::InitOrb(int & argc, char * argv[])
{
    if(!CORBA::is_nil(mOrb))
    {
        mOrb->destroy();
    }

    mNameService = CosNaming::NamingContext::_nil();  // can't use old ref with new orb
    mOrb = CORBA::ORB_init(argc, argv);
}

void CorbaUtil::ActivatePoaMgr()
{
// get reference to root POA
    CORBA::Object_var obj = mOrb->resolve_initial_references("RootPOA");
    PortableServer::POA_var poa = PortableServer::POA::_narrow(obj);
// activate POA manager
    PortableServer::POAManager_var mgr = poa->the_POAManager();
    mgr->activate();
}

void CorbaUtil::SetTimeout(int secs)
{
    CORBA::Object_var obj = mOrb->resolve_initial_references("ORBPolicyManager");
    CORBA::PolicyManager_var policy_manager = CORBA::PolicyManager::_narrow(obj.in());

    // disable default policies
    CORBA::PolicyList policy_list;
    policy_list.length(0);
    policy_manager->set_policy_overrides(policy_list, CORBA::SET_OVERRIDE);

    // setup a timeout policy
    TimeBase::TimeT timeout = secs * 10000000;
    CORBA::Any orb_timeout;
    orb_timeout <<= timeout;
    policy_list.length(1);
    policy_list[0] = mOrb->create_policy(
        Messaging::RELATIVE_RT_TIMEOUT_POLICY_TYPE, orb_timeout);

    // apply this policy
    policy_manager->set_policy_overrides(policy_list, CORBA::SET_OVERRIDE);
}

/*
Get the NameService object reference from the command line and decode
multiple profile IOR.
If there is no name service initial reference, the orb will
try to discover a name service by multicast.  This isn't desirable
as it just causes a delay, or possible selection of wrong name service.
Not sure how to disable it, though.
*/
bool CorbaUtil::InitNs()
{
    bool ok_so_far = true;
    CORBA::Object_var obj;
    try
    {
        obj = mOrb->resolve_initial_references("NameService");
    }
    catch (const CORBA::Exception & e)
    {
        ACE_DEBUG(( LM_ERROR, "No NameService reference available: %C\n", e._name() ));
        ok_so_far = false;
    }

// decode naming service multiple profiles
    if(ok_so_far)
    {
        TAO_MProfile & mp = obj->_stubobj()->base_profiles();
        TAO_Profile * profile;

        mNameServiceList.clear();
        unsigned int i;
        for(i=0;i<mp.profile_count();i++)
        {
            profile = mp.get_profile(i);
            mNameServiceList.push_back(profile->to_string());
        }
    }
    return ok_so_far;
}

/**
NB. caller is responsible for deallocating the char *
*/
char * CorbaUtil::ObjectToString(CORBA::Object_ptr obj)
{
    return mOrb->object_to_string(obj);
}

std::string CorbaUtil::ObjectToText(CORBA::Object_ptr obj)
{
    std::ostringstream ss;

    TAO_MProfile & mp = obj->_stubobj()->base_profiles();
    TAO_Profile * profile;

    for (CORBA::ULong i = 0; i < mp.profile_count(); i++)
    {
        profile = mp.get_profile(i);
        ss << (i == 0 ? "" : ", ") << profile->to_string();
    }
    return ss.str();
}

CORBA::Object_ptr CorbaUtil::StringToObject(const char * s)
{
    return mOrb->string_to_object(s);
}

/**
Provide an object reference for the name.
If one cannot be provided, return a nil reference.
Caller has responsibility for de-allocating the returned object.
*/
CORBA::Object_ptr CorbaUtil::ResolveObject(const CosNaming::Name & name)
{
    ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveObject()\n" ));

    CORBA::Object_ptr obj = CORBA::Object::_nil();

// check that NameService is ok
    if(CORBA::is_nil(mNameService))
    {
        ResolveNameService();
    }
    bool ok_so_far = !CORBA::is_nil(mNameService);

// try to resolve the object using the naming service
    if(ok_so_far)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mNsMutex);

        int retry;
        bool done;
        for(retry = 0, done = false; !done; ++retry)
        {
            try
            {
                obj = mNameService->resolve(name);
                done = true;
            }
            catch(CosNaming::NamingContext::NotFound & e)
            {
            // Naming service has no gateway of this name
                ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveObject() - Exception during resolve: %C\n",
                    e._name() ));
                ok_so_far = false;
                done = true;
            }
            catch(CORBA::COMM_FAILURE & e)
            {
            // COMM_FAILURE while communicating with naming serice
                ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveObject() - Exception during resolve: %C\n",
                    e._name() ));
                if(retry < 1)
                {
                    // allow a retry
                }
                else
                {
                    mNameService = CosNaming::NamingContext::_nil();
                        // set to nil so that next time we will ResolveNameService
                    ok_so_far = false;
                    done = true;
                }
            }
            catch(CORBA::Exception & e)
            {
            // Other problem while communicating with naming serice
                ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveObject() - Exception during resolve: %C\n",
                    e._name() ));
                mNameService = CosNaming::NamingContext::_nil();
                    // set to nil so that next time we will ResolveNameService
                ok_so_far = false;
                done = true;
            }
        }
    }

// prepare return value
    if (ok_so_far)
    {
    // object resolved
        ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveObject() - successful\n" ));
    }
    else
    {
    // object not resolved
        ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveObject() - failed\n" ));
        obj = CORBA::Object::_nil();
    }

    return obj;
}

/**
Try to set member mNameService to point to a working name service.
*/
void CorbaUtil::ResolveNameService()
{
    ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveNameService()\n" ));

    CORBA::Object_var obj;

// We check for working nameservice by trying to resolve the
// "ProductionAutomation" naming context.
    CosNaming::Name name;
    name.length(1);
    name[0].id = CORBA::string_dup("ProductionAutomation");

    ACE_Guard<ACE_Thread_Mutex> guard(mNsMutex);

// first see if we already have a working nameservice
    if(!CORBA::is_nil(mNameService))
    {
        try
        {
            obj = mNameService->resolve(name);
            ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveNameService() - name service ok\n" ));
            return;  // nothing to do, nameservice is OK
        }
        catch (const CORBA::Exception &)
        {
        }
    }

// go through the list to try to find a working nameservice
    CosNaming::NamingContext_var inc;
    unsigned int i;
    bool success;
    for(i=0, success=false; !success && i<mNameServiceList.size(); ++i)
    {
        ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveNameService() trying: %C\n", mNameServiceList[i].c_str() ));

        obj = mOrb->string_to_object(mNameServiceList[i].c_str());

// we use unchecked narrow as we will test the object ref below
        if(!CORBA::is_nil(obj))
        {
            inc = CosNaming::NamingContext::_unchecked_narrow(obj.in());
        }
        else
        {
            inc = CosNaming::NamingContext::_nil();
        }

// test the naming service
        if(!CORBA::is_nil(inc))
        {
            int retry;
            bool done;
            for(retry = 0, done = false; !done; ++retry)
            {
                try
                {
                    obj = inc->resolve(name);
                    success = true;
                    done = true;

                    // output debug info
                    ACE_DEBUG(( LM_DEBUG, "CorbaUtil::ResolveNameService() using:  %C\n", mNameServiceList[i].c_str() ));
                }
                catch(CORBA::TRANSIENT & e)
                {
                    ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveNameService() - Exception during resolve: %C\n",
                        e._name() ));

                    if(retry < 1)
                    {
                        // allow a retry
                    }
                    else
                    {
                        inc = CosNaming::NamingContext::_nil();
                        done = true;
                    }
                }
                catch(CORBA::COMM_FAILURE & e)
                {
                    ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveNameService() - Exception during resolve: %C\n",
                        e._name() ));

                    if(retry < 1)
                    {
                        // allow a retry
                    }
                    else
                    {
                        inc = CosNaming::NamingContext::_nil();
                        done = true;
                    }
                }
                catch(CORBA::Exception & e)
                {
                    ACE_DEBUG(( LM_ERROR, "CorbaUtil::ResolveNameService() - Exception during resolve: %C\n",
                        e._name() ));
                    inc = CosNaming::NamingContext::_nil();
                    done = true;
                }
            } // for loop for re-tries
        } // if(!CORBA::is_nil(inc))
    } // for loop going through name service list

    mNameService = inc;
}

void CorbaUtil::ClearNameServices()
{
    mNameServiceList.clear();

    // The following lines are important when working with a new orb as happens
    // when console is closed and re-opened.  Otherwise you use a reference
    // obtained from the old orb and things don't quite work properly.
    ACE_Guard<ACE_Thread_Mutex> guard(mNsMutex);
    mNameService = CosNaming::NamingContext::_nil();
}

void CorbaUtil::AddNameService(const char * string_obj_ref)
{
    mNameServiceList.push_back(string_obj_ref);
}

bool CorbaUtil::Advertise(CORBA::Object_ptr objref, CosNaming::Name & name)
{
    std::ostringstream ss;
    CORBA::Object_var obj;
    CosNaming::NamingContext_var inc;
    unsigned int i;
    bool success = false;
    for(i=0; i<mNameServiceList.size(); i++)
    {
        ss << mNameServiceList[i];
        obj = mOrb->string_to_object(mNameServiceList[i].c_str());
        if(advertise_i(objref, name, obj.in()))
        {
            ss << " - object advertised" << std::endl;
            success = true;
        }
        else
        {
            ss << " - object could not be advertised" << std::endl;
        }
    }

    ACE_DEBUG(( LM_INFO, ss.str().c_str() ));

    return success;
}

void CorbaUtil::Unadvertise(CosNaming::Name & name)
{
    std::ostringstream ss;
    CORBA::Object_var obj;
    CosNaming::NamingContext_var inc;
    unsigned int i;
    for(i=0; i<mNameServiceList.size(); i++)
    {
        ss << mNameServiceList[i];
        obj = mOrb->string_to_object(mNameServiceList[i].c_str());
        if(unadvertise_i(name, obj.in()))
        {
            ss << " - object removed" << std::endl;
        }
        else
        {
            ss << " - object could not be removed" << std::endl;
        }
    }
    ACE_DEBUG(( LM_INFO, ss.str().c_str() ));
}

/**
Advertise an object reference in a naming service.
You can use names with length
greater than 1.
Naming contexts are created if necessary.
@param objref the object reference to be advertised
@param name the name to be used
@param ns_obj the naming context in which to advertise the object
@return true for success, false otherwise
*/
bool CorbaUtil::advertise_i(CORBA::Object_ptr objref, CosNaming::Name & name, CORBA::Object_ptr ns_obj)
{
    bool success;
    CosNaming::Name localname;
    CORBA::Object_var obj;
    CosNaming::NamingContext_var inc;
    CosNaming::NamingContext_var nc;

// narrow the naming service object reference
    try
    {
        inc = CosNaming::NamingContext::_narrow(ns_obj);
        success = true;
    }
    catch(const CORBA::Exception &)
    {
        success = false;
    }


// find/create the context
    if(success)
    {
        for(unsigned int i=0;i<name.length()-1;i++)
        {
            localname.length(i+1);
            localname[i] = name[i];

            try
            {
                obj = inc->resolve(localname);
                nc = CosNaming::NamingContext::_narrow(obj.in());
            }
            catch (const CosNaming::NamingContext::NotFound &)
            {
            // if the naming context doesn't exist, create it
                try
                {
                    nc = inc->bind_new_context(localname);
                }
                catch (const CosNaming::NamingContext::AlreadyBound &)
                {
                // another gateway has just bound the context
                // this can happen when main/reserve are started simultaneously
                // e.g. as services
                    obj = inc->resolve(localname);
                    nc = CosNaming::NamingContext::_narrow(obj.in());
                }
            }
            catch (const CORBA::Exception &)
            {
            // some problem with the naming service, return a nil reference
                nc = CosNaming::NamingContext::_nil();
                break;
            }
        }
    }
    else
    {
        nc = CosNaming::NamingContext::_nil();
    }

// bind the object reference
    if(!CORBA::is_nil(nc)) {
        localname.length(1);
        localname[0] = name[name.length()-1];
        try {
            nc->rebind(localname, objref);
            success = true;
        }
        catch (const CORBA::Exception &) {
            success = false;
        }
    } else {
        success = false;
    }

    return success;
}

/**
Remove an advertised  object reference from a naming context.
@param name the name used
@param ns_obj the naming context from which to remove the named object reference
@return true for success, false otherwise
*/
bool CorbaUtil::unadvertise_i(CosNaming::Name & name, CORBA::Object_ptr ns_obj)
{
    if(name.length() < 2)
    {
        // name invalid
        return false;
    }

    // strip off last element of name to get naming context
    CosNaming::Name localname;
    localname.length(name.length()-1);
    for(unsigned int i=0;i<name.length()-1;i++) {
        localname[i] = name[i];
    }

    // get initial naming context
    CosNaming::NamingContext_var inc;
    bool try_again = false;
    bool failed = false;
    CORBA::Object_var obj;
    try
    {
        inc = CosNaming::NamingContext::_narrow(ns_obj);
        obj = inc->resolve(localname);
    }
    catch (const CORBA::COMM_FAILURE &)
    {
        try_again = true;
    }
    catch (const CORBA::TRANSIENT &)
    {
        try_again = true;
    }
    catch (const CORBA::Exception &)
    {
        failed = true;
    }
    if(try_again)
    {
        try
        {
            inc = CosNaming::NamingContext::_narrow(ns_obj);
            obj = inc->resolve(localname);
        }
        catch (const CORBA::Exception &)
        {
            failed = true;
        }
    }
    if(failed)
    {
    // NameService not responding
        return false;
    }

    // Get context for our name and remove object
    CosNaming::NamingContext_var nc;
    try
    {
        nc = CosNaming::NamingContext::_narrow(obj);
        localname.length(name.length());
        localname[name.length()-1] = name[name.length()-1];
    // remove binding to object
        inc->unbind(localname);
    }
    catch (const CORBA::Exception &)
    {
    // NameService not responding
        return false;
    }

    // remove context if empty, i.e. not another main/reserve there
    try
    {
    // destroy context
        nc->destroy();  // may throw NotEmpty exception
    // remove binding to context
        localname.length(name.length()-1);
        inc->unbind(localname);
    }
    catch (const CosNaming::NamingContext::NotEmpty &)
    {
    // exception occurs if we attempt to remove service from name server
    // when main/reserve still present.
    // No action required as we don't want to remove context in this case.
    }
    catch (const CORBA::Exception &)
    {
    // NameService not responding
        return false;
    }

    // done
    return true;
}

bool CorbaUtil::GetRecorderList(std::vector<std::string> & svclist)
{
    const CORBA::ULong chunk_size = 200;

    svclist.clear();
    //svclist.push_back("hello world");


// get RecordingDevices naming context
    CosNaming::Name name;
    name.length(2); 
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("RecordingDevices");

    CORBA::Object_var obj = ResolveObject(name);
    bool ok_so_far = !CORBA::is_nil(obj.in());

// narrow the object reference
    CosNaming::NamingContext_var gnc;
    if(ok_so_far)
    {
        try
        {
            gnc = CosNaming::NamingContext::_narrow(obj);
        }
        catch(CORBA::Exception & e)
        {
        // _narrow tried to contact the naming service but there was an exception
            ACE_DEBUG(( LM_ERROR, "CorbaUtil::GetServiceList() - Exception during narrow: %C\n",
                e._name() ));
            ok_so_far = false;
        }
        if(CORBA::is_nil(gnc))
        {
        // object could not be narrowed to a naming context
            ok_so_far = false;
        }
    }
    

    CosNaming::BindingIterator_var it;
    CosNaming::BindingList_var bl;
    if(ok_so_far)
    {
        try
        {
            gnc->list(chunk_size, bl, it);
        }
        catch (const CORBA::Exception & e)
        {
            ACE_DEBUG(( LM_ERROR, "CorbaUtil::GetServiceList() - Exception during list: %C\n",
                e._name() ));
            ok_so_far = false;
        }
    }

    // If we get this far, we have successfully done an initial list
    CORBA::ULong i;
    if(ok_so_far)
    {
        for (i=0; i<bl->length(); i++)
        {
            svclist.push_back(bl[i].binding_name[0].id.in());
        }
    }

    // We now have to see if there was an iterator for further items

    if (ok_so_far && !CORBA::is_nil(it))
    {
        try
        {
            while(it->next_n(chunk_size, bl))
            {
                for (i=0; i<bl->length(); i++)
                {
                    svclist.push_back(bl[i].binding_name[0].id.in());
                }
            }
        }
        catch(const CORBA::Exception &)
        {
        // never mind, at least we got the initial list
        }
        it->destroy();
    }

    //if(!ok_so_far)
    //{
    //  svclist.push_back("Naming Service Error!");
    //}
    
    return ok_so_far;
}


bool CorbaUtil::ListContext(const CosNaming::Name & name, std::vector<std::string> & svclist)
{
    const CORBA::ULong chunk_size = 200;

    svclist.clear();
    //svclist.push_back("hello world");


// get naming context
    CORBA::Object_var obj = ResolveObject(name);
    bool ok_so_far = !CORBA::is_nil(obj.in());

// narrow the object reference
    CosNaming::NamingContext_var gnc;
    if(ok_so_far)
    {
        try
        {
            gnc = CosNaming::NamingContext::_narrow(obj);
        }
        catch(CORBA::Exception & e)
        {
        // _narrow tried to contact the naming service but there was an exception
            ACE_DEBUG(( LM_ERROR, "CorbaUtil::GetServiceList() - Exception during narrow: %C\n",
                e._name() ));
            ok_so_far = false;
        }
        if(CORBA::is_nil(gnc))
        {
        // object could not be narrowed to a naming context
            ok_so_far = false;
        }
    }
    

    CosNaming::BindingIterator_var it;
    CosNaming::BindingList_var bl;
    if(ok_so_far)
    {
        try
        {
            gnc->list(chunk_size, bl, it);
        }
        catch (const CORBA::Exception & e)
        {
            ACE_DEBUG(( LM_ERROR, "CorbaUtil::GetServiceList() - Exception during list: %C\n",
                e._name() ));
            ok_so_far = false;
        }
    }

    // If we get this far, we have successfully done an initial list
    CORBA::ULong i;
    if(ok_so_far)
    {
        for (i=0; i<bl->length(); i++)
        {
            svclist.push_back(bl[i].binding_name[0].id.in());
        }
    }

    // We now have to see if there was an iterator for further items

    if (ok_so_far && !CORBA::is_nil(it))
    {
        try
        {
            while(it->next_n(chunk_size, bl))
            {
                for (i=0; i<bl->length(); i++)
                {
                    svclist.push_back(bl[i].binding_name[0].id.in());
                }
            }
        }
        catch(const CORBA::Exception &)
        {
        // never mind, at least we got the initial list
        }
        it->destroy();
    }

    //if(!ok_so_far)
    //{
    //  svclist.push_back("Naming Service Error!");
    //}
    
    return ok_so_far;
}
