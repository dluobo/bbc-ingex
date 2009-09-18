/*
 * $Id: CorbaUtil.h,v 1.3 2009/09/18 15:48:10 john_f Exp $
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

#ifndef CorbaUtil_h
#define CorbaUtil_h

#include <orbsvcs/CosNamingC.h>
#include <ace/Thread_Mutex.h>
#include <vector>
#include <string>

/**
Singleton class that provides miscellaneous CORBA functions
*/
class CorbaUtil
{
public:
// methods
    static CorbaUtil * Instance()
    {
        if (mInstance == 0)  // is it the first call?
        {  
            mInstance = new CorbaUtil; // create sole instance
        }
        return mInstance;// address of sole instance
    }
    void InitOrb(int & argc, char * argv[]);
    void DestroyOrb();
    void ActivatePoaMgr();
    void SetTimeout(int secs);
    bool InitNs();

    void OrbRun(ACE_Time_Value timeout)
    {
    // NB. CORBA::ORB::run takes a reference to the ACE_Time_Value and
    // will change it.  Our function, however, takes a value so the
    // parameter won't be changed.  Behaviour should be same even
    // with inlining.
        mOrb->run(timeout);
    }
    bool OrbWorkPending()
    {
        return mOrb->work_pending();
    }
    void OrbPerformWork(ACE_Time_Value timeout)
    {
    // comments as for OrbRun above
        mOrb->perform_work(timeout);
    }
    void OrbShutdown()
    {
        mOrb->shutdown(1);
    }
    void OrbDestroy()
    {
        mOrb->destroy();
    }
    char * ObjectToString(CORBA::Object_ptr obj);
    std::string ObjectToText(CORBA::Object_ptr obj);
    CORBA::Object_ptr StringToObject(const char * s);

    TAO_ORB_Core * OrbCore() { return mOrb->orb_core(); }

    CORBA::Object_ptr ResolveObject(const CosNaming::Name & name);
    void ClearNameServices();
    void AddNameService(const char * string_obj_ref);
    bool Advertise(CORBA::Object_ptr objref, CosNaming::Name & name);
    void Unadvertise(CosNaming::Name & name);
    bool GetRecorderList(std::vector<std::string> & svclist);
    bool ListContext(const CosNaming::Name & name, std::vector<std::string> & svclist);
protected:
    // Protect constructors to force use of Instance()
    CorbaUtil();
    CorbaUtil(const CorbaUtil &);
    CorbaUtil & operator= (const CorbaUtil &);

private:
// methods
    void ResolveNameService();
    bool advertise_i(CORBA::Object_ptr objref, CosNaming::Name & name,
        CORBA::Object_ptr ns_obj);
    bool unadvertise_i(CosNaming::Name & name,
        CORBA::Object_ptr ns_obj);
// variables
    static CorbaUtil * mInstance;
    CORBA::ORB_var mOrb;
    std::vector<std::string> mNameServiceList;
    ACE_Thread_Mutex mNsMutex; ///< mutex to prevent concurrent use of ns
    CosNaming::NamingContext_var mNameService;
};

#endif //#ifndef CorbaUtil_h
