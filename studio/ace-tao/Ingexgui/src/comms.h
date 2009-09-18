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

#ifndef _COMMS_H_
#define _COMMS_H_
//#include "ingexgui.h"
#include <wx/wx.h> //must include before CosNamingC.h or something is defined twice in mingw
#include "orbsvcs/CosNamingC.h"
#include "RecorderC.h"

/**
Handles communication with the name server in a threaded manner.
*/

class Comms : public wxThread
{
public:
	Comms(wxWindow*, int&, char**);
	~Comms();
	void StartGettingRecorders(WXTYPE, int);
	bool GetStatus(wxString & errMsg);
	void GetRecorderList(wxArrayString& list);
	wxString SelectRecorder(wxString, ::ProdAuto::Recorder_var &);
private:
	wxWindow * mParent;
	void SetTimeout(int secs);
	bool InitNs();
	CORBA::Object_ptr ResolveObject(const CosNaming::Name & name, wxString & msg);
	ExitCode Entry();
	WXTYPE mEventType;
	int mEventId;
	wxMutex mMutex;
	bool mDie;
	wxCondition* mCondition;
	CosNaming::NamingContext_var mNameService;
	bool mOK;
	CORBA::ORB_var mOrb;
	wxArrayString mNameServiceList, mRecorderList;
	wxString mErrMsg;
	ACE_Thread_Mutex mNsMutex; ///< mutex to prevent concurrent use of ns
};


#endif
