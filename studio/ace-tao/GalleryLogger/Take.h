/*
 * $Id: Take.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to represent a "take".
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

#ifndef Take_h
#define Take_h

#include <string>
#include <vector>

#include "../../database/src/Take.h"  // for prodauto::Take
#include "Timecode.h"
//#include "RecordedSource.h"

class Take
{
public:
    enum ResultEnum { UNKNOWN, NG, GOOD };

    /// Default constructor
    Take();

    /// Construction from a database take
    Take(const prodauto::Take * pt);

    /// Assignment from a database take
    const Take & operator=(const prodauto::Take *);

    /// Copy to a database take
    void CopyTo(prodauto::Take *) const;

    void Clear();

    void Number(int n) { mNumber = n; }
    int Number() const { return mNumber; }

    void Comment(const char * s) { mComment = s; }
    const char * Comment() const { return mComment.c_str(); }
    bool HasComment() const { return !mComment.empty(); }

    void In(Timecode & tc) { mInTime = tc; }
    Timecode In() const { return mInTime; }

    void Out(Timecode & tc) { mOutTime = tc; }
    Timecode Out() const { return mOutTime; }

    //Timecode Duration() const { return mOutTime - mInTime; }

    std::string DateText() const;

    void Result(ResultEnum r) { mResult = r; }
    ResultEnum Result() { return mResult; }
    void IsGood(bool b) { mResult = (b ? GOOD : NG); }
    bool IsGood() const { return mResult == GOOD; }
    const char * ResultText();
    long Location() const { return mLocation; }

    void Year(int y) { mYear = y; }
    int Year() const { return mYear; }
    void Month(int m) { mMonth = m; }
    int Month() const { return mMonth; }
    void Day(int d) { mDay = d; }
    int Day() const { return mDay; }

    //const char * Sources();

    //int SourceCount() const { return mRecordedSources.size(); }
    //::RecordedSource & RecordedSource(int i) { return mRecordedSources[i]; }
    //const ::RecordedSource & RecordedSource(int i) const { return mRecordedSources[i]; }
    //void AddRecordedSource(const ::RecordedSource & rs) { mRecordedSources.push_back(rs); }
    //::RecordedSource & LastRecordedSource() { return mRecordedSources.back(); }

private:
    int mNumber;
    std::string mComment;
    ResultEnum mResult;
    Timecode mInTime;
    Timecode mOutTime;
    std::string mDate;
    int mYear;
    int mMonth;
    int mDay;
    long mLocation;
    std::string mLocationText;

    //std::vector<::RecordedSource> mRecordedSources;

    //std::string mSources; // Used only for const char * return value;
};

#endif //#ifndef Take_h
