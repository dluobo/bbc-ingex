/***************************************************************************
 *   Copyright (C) 2006-2008 British Broadcasting Corporation              *
 *   - all rights reserved.                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "comms.h"

#include "tao/corba.h"
#include "tao/PortableServer/PortableServer.h"
#include "tao/Stub.h"		// needed for decoding mutliple profile iors
#include "tao/Profile.h"	// needed for decoding mutliple profile iors
#include "tao/TimeBaseC.h"
#include "tao/Messaging/Messaging.h"


/// Initialises ORB and naming service, and starts thread which obtains list of recorders.
/// @param parent the parent window (to where events will be sent)
/// @param argc Command line argument count - see argv.
/// @param argv Command line arguments - the ORB will remove and respond to any it recognises.
Comms::Comms(wxWindow * parent, int argc, wxChar** argv)
: wxThread(wxTHREAD_JOINABLE), mParent(parent), mNameService(CosNaming::NamingContext::_nil()), mOK(true), mErrMsg(wxT("Not Initialised")) //joinable thread means we can terminate it in order to be able to delete mCondition safely
{
	if (argc < 3) {
		wxMessageDialog dlg(mParent, wxT("To connect to recorders requires nameserver details on the command line, such as \"-ORBDefaultInitRef corbaloc:iiop:192.168.1.123:8888\"."), wxT("Comms problem"), wxICON_EXCLAMATION | wxOK); //NB not using wxMessageBox because (in GTK) it doesn't stop the parent window from being selected, so it can end up hidden, making the app appear to have hanged
		dlg.ShowModal();
		mOK = false;
	}
	if (mOK) {
		// create char * [] version of argv
//		char * argv_[argc]; //gcc likes this but Win32 doesn't
		char ** argv_ = new char * [argc];
		for (int i = 0; i < argc; i++) {
			wxString wxArg = argv[i];
			char * arg = new char[wxArg.Len() + 1];
			strcpy(arg, wxArg.mb_str());
			argv_[i] = arg;
		}
		// Initialise ORB
		try {
			mOrb = CORBA::ORB_init(argc, argv_);
			SetTimeout(5); //Can lock up for a long time if this isn't set
		// Initialise naming service, supplied to the orb in the command line args
		}
		catch (const CORBA::Exception & e) {
			wxString msg(e._name(), *wxConvCurrent);
			msg = wxT("Failed to initialise ORB: ") + msg;
			wxMessageDialog dlg(mParent, msg, wxT("Comms problem"), wxICON_EXCLAMATION | wxOK);
			dlg.ShowModal();
			mOK = false;
		}
		delete[] argv_;
	}
	if (mOK && !InitNs()) {
		wxMessageDialog dlg(mParent, wxT("Failed to initialise naming service."), wxT("Comms problem"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		mOK = false;
	}
	mCondition = new wxCondition(mMutex);
	if (mOK && wxTHREAD_NO_ERROR != Create()) {
		wxMessageDialog dlg(mParent, wxT("Failed to create comms thread."), wxT("Comms problem"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		Delete(); //free up memory used by the thread
		delete mCondition;
		mOK = false;
	}
	if (mOK) {
		mMutex.Lock(); //to detect that thread is ready to accept signals
	}
	if (mOK && wxTHREAD_NO_ERROR != Run()) {
		wxMessageDialog dlg(mParent, wxT("Failed to run comms thread."), wxT("Comms problem"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		Delete(); //free up memory used by the thread
		delete mCondition;
		mOK = false;
	}
	if (mOK) { //thread is starting
		mMutex.Lock(); //wait for thread to be ready to accept signals
		mMutex.Unlock();
	}
}

void Comms::SetTimeout(int secs)
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

/// Shuts down thread (if running) to avoid memory leaks.
Comms::~Comms()
{
 	if (mOK) {
 		//the thread's running and mCondition exists
 		mMutex.Lock();
 		mDie = true;
 		mCondition->Signal(); //doesn't matter if this gets lost because thread will still be doing the previous loop
 		mMutex.Unlock();
 		Wait();
 		delete mCondition;
 	}
}

/// Get the NameService object reference from the command line and decode multiple profile IOR.
/// If there is no name service initial reference, the orb will try to discover a name service by multicast.
/// This isn't desirable as it just causes a delay, or possible selection of wrong name service.
/// Not sure how to disable it, though.
bool Comms::InitNs()
{
	bool ok_so_far = true;
	CORBA::Object_var obj;
	try
	{
		obj = mOrb->resolve_initial_references("NameService");
	}
	catch (const CORBA::Exception & e)
	{
		wxString msg(e._name(), *wxConvCurrent);
		msg = wxT("No NameService reference available: ") + msg;
		wxMessageDialog dlg(mParent, msg, wxT("Comms problem"), wxICON_EXCLAMATION | wxOK);
		dlg.ShowModal();
		ok_so_far = false;
	}

// decode naming service multiple profiles
	if(ok_so_far)
	{
		TAO_MProfile & mp = obj->_stubobj()->base_profiles();

		mNameServiceList.clear();
		unsigned int i;
		for(i=0;i<mp.profile_count();i++)
		{
			mNameServiceList.Add(wxString(mp.get_profile(i)->to_string(), *wxConvCurrent));
		}
	}
	return ok_so_far;
}

/// Starts looking for list of recorders in the background.
/// @param eventType The command event type to issue once finished.
/// @param id The ID for the command event to issue once finished.
void Comms::StartGettingRecorders(WXTYPE eventType, int id)
{
	wxMutexLocker lock(mMutex); //prevent a signal being sent unless it's waiting for it or it's already looking for recorders (or action would not be triggered), and prevents simultaneous access to the variables below
	mEventType = eventType;
	mEventId = id;
	mDie = false;
	mCondition->Signal(); //doesn't matter if this gets lost because thread will still be doing the previous loop
}

/// Indicates whether the object is happy.
/// @param errMsg This will return empty unless there is a problem, whereupon it will contain a description of the error.
/// @return True if OK.
bool Comms::GetStatus(wxString & errMsg)
{
	wxMutexLocker lock(mMutex);
	errMsg = mErrMsg;
	return mOK;
}

/// Provides the list of recorder names from the last time the name server was interrogated.
/// @param list Returns the recorder names.
void Comms::GetRecorderList(wxArrayString& list)
{
	wxMutexLocker lock(mMutex);
	list = mRecorderList;
}

/// Thread entry point, called by wxWidgets.
/// @return Always 0.
wxThread::ExitCode Comms::Entry()
{
	//assumes mutex already locked here, so can signal that thread has started by unlocking it when Wait() is called
	mDie = false;
	while (1) {
 		if (mDie) {
 			//asked to die while thread running
 			Exit();
 		}
		mCondition->Wait(); //unlock (if locked) and wait for a signal
		mMutex.Unlock(); //prevent CORBA calls blocking - doesn't matter if a signal is lost at this point
 		if (mDie) {
 			//asked to die
 			Exit();
 		}

		//DO THE WORK
		const CORBA::ULong chunk_size = 200;

// get RecordingDevices naming context
		CosNaming::Name name;
		name.length(2);	
		name[0].id = CORBA::string_dup("ProductionAutomation");
		name[1].id = CORBA::string_dup("RecordingDevices");

// first see if we already have a working nameservice
		if(!CORBA::is_nil(mNameService))
		{
		try
			{
//				CORBA::Object_ptr obj = // CORBA::Object::_nil();
				mNameService->resolve(name);
//				ACE_DEBUG(( LM_INFO, "CorbaMisc::ResolveNameService() - name service ok\n" ));
//				return;  // nothing to do, nameservice is OK
			}
			catch (const CORBA::Exception &)
			{
				mNameService = CosNaming::NamingContext::_nil();
			}
		}
// look for working nameservice
		if(CORBA::is_nil(mNameService))
		{
			CORBA::Object_var obj;

			// We check for working nameservice by trying to resolve the
			// "ProductionAutomation" naming context.
			CosNaming::Name name;
			name.length(1);
			name[0].id = CORBA::string_dup("ProductionAutomation");

			ACE_Guard<ACE_Thread_Mutex> guard(mNsMutex);

			// go through the list to try to find a working nameservice
			CosNaming::NamingContext_var inc;
			unsigned int i;
			bool success;
			for(i=0, success=false; !success && i<mNameServiceList.size(); ++i)
			{
				obj = mOrb->string_to_object(mNameServiceList[i].mb_str(*wxConvCurrent));

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
		//needed in Win32 to prevent crashing on an exception
		try {
			throw 123;
		}
		catch (int) {
		}
						try
						{
							obj = inc->resolve(name);
							success = true;
							done = true;
	
						// output debug info
//						ACE_DEBUG(( LM_INFO, "CorbaMisc::ResolveNameService() using:  %C\n", mNameServiceList[i].c_str() ));
						}
						catch(CORBA::TRANSIENT & e)
						{
//						ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveNameService() - Exception during resolve: %C\n",
//							e._name() ));
	
							if(retry < 1)
							{
								// allow a retry
							}
							else
							{
								mMutex.Lock();
								mErrMsg = wxT("Exception resolving naming service: ") + wxString(e._name(), *wxConvCurrent);
								mErrMsg += wxT("\nThis may resolve itself if you try again.\nIs there a name server running on the name server machine(s)?");
								mMutex.Unlock();
								inc = CosNaming::NamingContext::_nil();
								done = true;
							}
						}
						catch(CORBA::COMM_FAILURE & e)
						{
//							ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveNameService() - Exception during resolve: %C\n",
//								e._name() ));
	
							if(retry < 1)
							{
								// allow a retry
							}
							else
							{
								mMutex.Lock();
								mErrMsg = wxT("COMM_FAILURE exception resolving naming service: ") + wxString(e._name(), *wxConvCurrent);
								mMutex.Unlock();
								inc = CosNaming::NamingContext::_nil();
								done = true;
							}
						}
						catch(CORBA::Exception & e)
						{
//							ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveNameService() - Exception during resolve: %C\n",
//								e._name() ));
							mMutex.Lock();
							mErrMsg = wxT("Exception resolving naming service: ") + wxString(e._name(), *wxConvCurrent);
							mErrMsg += wxT("\nIs the name server machine on and connected to the network?  Are there any recorders?\n\nThis may resolve itself if you try again.");
							mMutex.Unlock();
							inc = CosNaming::NamingContext::_nil();
							done = true;
						}
					} // for loop for re-tries
				} // if(!CORBA::is_nil(inc))
			} // for loop going through name service list
	
			mNameService = inc;
		}
		mOK = !CORBA::is_nil(mNameService);

		CORBA::Object_var obj; //_var deallocates automatically on deletion
// try to resolve the object using the naming service
		if(mOK)
		{
			wxString msg;
			obj = ResolveObject(name, msg);
			mMutex.Lock();
			mErrMsg = msg;
			mMutex.Unlock();
			mOK = !CORBA::is_nil(obj.in());
		}

	// prepare return value
		if(mOK)
		{
		// object resolved
//			ACE_DEBUG(( LM_INFO, "CorbaMisc::ResolveObject() - successful\n" ));
		}
		else
		{
		// object not resolved
//			ACE_DEBUG(( LM_INFO, "CorbaMisc::ResolveObject() - failed\n" ));
			obj = CORBA::Object::_nil();
		}	

// narrow the object reference
		CosNaming::NamingContext_var gnc;
		if(mOK)
		{
			try
			{
				gnc = CosNaming::NamingContext::_narrow(obj);
			}
			catch(CORBA::Exception & e)
			{
			// _narrow tried to contact the naming service but there was an exception
//				ACE_DEBUG(( LM_ERROR, "CorbaMisc::GetServiceList() - Exception during narrow: %C\n",
//					e._name() ));
				mMutex.Lock();
				mErrMsg = wxT("Exception during narrow: ") + wxString(e._name(), *wxConvCurrent);
				mErrMsg += wxT("\nTry again.");
				mMutex.Unlock();
				mOK = false;
			}
			if(CORBA::is_nil(gnc))
			{
			// object could not be narrowed to a naming context
				mOK = false;
			}
		}

		CosNaming::BindingIterator_var it;
		CosNaming::BindingList_var bl;
		if(mOK)
		{
			try
			{
				gnc->list(chunk_size, bl, it);
			}
			catch (const CORBA::Exception & e)
			{
//				ACE_DEBUG(( LM_ERROR, "CorbaMisc::GetServiceList() - Exception during list: %C\n",
//					e._name() ));
				mMutex.Lock();
				mErrMsg = wxT("Exception during list: ") + wxString(e._name(), *wxConvCurrent);
				mErrMsg += wxT("\nTry again.");
				mMutex.Unlock();
				mOK = false;
			}
		}
	
		// If we get this far, we have successfully done an initial list
		CORBA::ULong i;
		mRecorderList.Clear();
		if(mOK)
		{
			for (i=0; i<bl->length(); i++)
			{
				mMutex.Lock();
				mRecorderList.Add(wxString(bl[i].binding_name[0].id.in(), *wxConvCurrent));
				mMutex.Unlock();
			}
		}
	
		// We now have to see if there was an iterator for further items
	
		if (mOK && !CORBA::is_nil(it))
		{
			try
			{
				while(it->next_n(chunk_size, bl))
				{
					for (i=0; i<bl->length(); i++)
					{
						mMutex.Lock();
						mRecorderList.Add(wxString(bl[i].binding_name[0].id.in(), *wxConvCurrent));
						mMutex.Unlock();
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
		//	svclist.push_back("Naming Service Error!");
		//}

//		mOK = ok;
		mMutex.Lock();
		wxCommandEvent event(mEventType, mEventId);
		mParent->AddPendingEvent(event);
	}
	return 0;
}

/// Provide an object reference for the name.
/// This function is thread safe.
/// @param name The name to look up.
/// @param msg Returns empty if no problem, or a description of the error otherwise.
/// @return The resolved object, or a nil reference if unsuccessful.  Caller has responsibility for de-allocating this.
CORBA::Object_ptr Comms::ResolveObject(const CosNaming::Name & name, wxString & msg) {
	CORBA::Object_ptr obj = CORBA::Object::_nil();
	ACE_Guard<ACE_Thread_Mutex> guard(mNsMutex);
	int retry;
	bool done;
	if (!CORBA::is_nil(mNameService)) { //sanity check
		for(retry = 0, done = false; !done; ++retry)
		{
			try
			{
				obj = mNameService->resolve(name);
				done = true;
			}
			catch(CosNaming::NamingContext::NotFound &)
			{
			// Naming service has no recorder of this name
	//			ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveObject() - Exception during resolve: %C\n",
	//				e._name() ));
				msg = wxT("Category not found in naming service.");// + wxString(e._name(), *wxConvCurrent);
				msg += wxT("\nAre there recorders available, and is this the right name server?");
				done = true;
			}
			catch(CORBA::COMM_FAILURE & e)
			{
				// COMM_FAILURE while communicating with naming service
	//				ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveObject() - Exception during resolve: %C\n",
	//					e._name() ));
				if(retry < 1)
				{
					// allow a retry
				}
				else
				{
					mNameService = CosNaming::NamingContext::_nil();
						// set to nil so that next time we will ResolveNameService
					msg = wxT("Exception communicating with name server: ") + wxString(e._name(), *wxConvCurrent);
					msg += wxT("\nTry again.");
					done = true;
				}
			}
			catch(CORBA::Exception &)
			{
				// Other problem while communicating with naming service
	//				ACE_DEBUG(( LM_ERROR, "CorbaMisc::ResolveObject() - Exception during resolve: %C\n",
	//					e._name() ));
	//			mNameService = CosNaming::NamingContext::_nil();
				// set to nil so that next time we will ResolveNameService
				done = true;
			}
		}
	}
	else {
		msg = wxT("I do not have a naming service!");
	}
	return obj;
}

/// Returns a recorder object corresponding to the supplied name.
/// This function is thread safe.
/// @param recorder The name of the recorder.
/// @param rec_object The resolved object, or a nil reference if unsuccessful.
/// @return Empty if no problem, or a description of the error otherwise.
wxString Comms::SelectRecorder(wxString recorder, ::ProdAuto::Recorder_var & rec_object)
{
	wxString msg;
	rec_object = ::ProdAuto::Recorder::_nil();
	// get object reference from given name
	CosNaming::Name objectName;
	objectName.length(3);
	objectName[0].id = CORBA::string_dup("ProductionAutomation");
	objectName[1].id = CORBA::string_dup("RecordingDevices");
	objectName[2].id = CORBA::string_dup(recorder.mb_str(*wxConvCurrent));
	CORBA::Object_ptr object = ResolveObject(objectName, msg); //we have to de-allocate this
	if (CORBA::is_nil(object)) {
		msg = wxT("Couldn't resolve recorder object.  The recorder may have shut down since the list of recorders was refreshed.");
	}
	else {
		try {
			rec_object = ::ProdAuto::Recorder::_narrow(object);
		}
		catch (const CORBA::Exception & e) {
			msg = wxT("Exception during narrow: ") + wxString(e._name(), *wxConvCurrent) + wxT(".");
		}
	}
	if (msg.IsEmpty()) {
		if (CORBA::is_nil(object)) {
			msg = wxT("Not a recording device.");
		}
	}
	delete object;
	return msg;
}

