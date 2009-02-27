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

#ifndef _DRAGBUTTONLIST_H_
#define _DRAGBUTTONLIST_H_

#include <wx/wx.h>
#include <vector>
#include <string>
#include "RecorderC.h"

class TakeInfo;

/// Ignore the "drag" bit - this class represents a set of source selection buttons for playback.
class DragButtonList : public wxScrolledWindow
{
	public:
		DragButtonList(wxWindow *);
		void SetTracks(TakeInfo &, std::vector<std::string> &, std::vector<std::string> &);
		const wxString SetMXFFiles(wxArrayString &, std::vector<std::string> &, std::vector<std::string> &, ProdAuto::MxfTimecode &);
		void SetEtoE(std::vector<std::string> &, std::vector<std::string> &);
		void EnableAndSelectTracks(std::vector<bool> *, const unsigned int);
		void Clear();
		bool EarlierTrack(bool);
		bool LaterTrack(bool);
		void SelectQuadrant(unsigned int source);
//		std::vector<std::string> * GetFiles();
	private:
		wxBoxSizer * mSizer;
};

#endif
