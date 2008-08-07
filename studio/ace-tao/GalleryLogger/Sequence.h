/*
 * $Id: Sequence.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to represent a sequence from a programme script.
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

#ifndef Sequence_h
#define Sequence_h

#pragma warning(disable:4786) // // To suppress some <vector>-related warnings

#include <string>
#include <vector>
#include "Take.h"
#include "Timecode.h"

#include "ace/Bound_Ptr.h"
#include "ace/Null_Mutex.h"

/**
This represents a reference to a particular point in a script.
It will usually be a sequence or scene number but these are not
necessarily numeric (e.g. Scene 14a) so we have to use a string.
That makes it harder to get script order by sorting but we will
get script order from script eventually.
*/
class ScriptRef
{
public:
// enum
    enum EnumeratedType { SEQUENCE, SCENE, SHOT };
// constructor
    ScriptRef(const char * v) : mValue(v), mType(SEQUENCE) {}
// methods
    EnumeratedType Type() const { return mType; }
    const char * Value() const { return mValue.c_str(); }
    void Value(const char * s) { mValue = s; }
private:
    EnumeratedType mType;
    std::string mValue;
};

class Sequence
{
public:
    //Sequence(int number, const char * name, const char * id);
    Sequence(const char * name,
        const ScriptRef & start,
        //const ScriptRef & end,
        const char * id);

    void Name(const char * s) { mName = s; }
    const char * Name() const { return mName.c_str(); }
    const char * Name(int max);

    void UniqueId(const char * id) { mUniqueId = id; }
    const char * UniqueId() { return mUniqueId.c_str(); }

    //void ProgrammeIndex(int n) { mProgrammeIndex = n; }
    //int ProgrammeIndex() { return mProgrammeIndex; }
    //const char * ProgrammeIndexT();

    void TargetDuration(const Duration & t) { mTargetDuration = t; }
    const Duration & TargetDuration() { return mTargetDuration; }

    int TakeCount() { return mTakes.size(); }
    ::Take & Take(int i) { return mTakes[i]; }
    ::Take & LastTake() { return mTakes.back(); }
    void AddTake(const ::Take & take);
    void ModifyTake(int index, const ::Take & take);
    bool HasGoodTake();

    ScriptRef & Start() { return mScriptStart; }
    //ScriptRef & End() { return mScriptEnd; }

    std::string ScriptRefRange();

private:
// Data held in this object
    std::string mName; // Descriptive text.
    ScriptRef mScriptStart; // Now simply a string identifying relevant points in script
                            // e.g. "12" or "12-13" or "12, 18, 24" etc.
    //ScriptRef mScriptEnd;    // No longer used.
    std::string mUniqueId;  // Can be used in a filename, for example.
    Duration mTargetDuration;  // Target duration.
    //int mNumber;
    //char mNumberText[10];
    //int mProgrammeIndex;
    //char mProgrammeIndexText[10];
    std::vector<::Take> mTakes;  // Takes.
};

typedef ACE_Strong_Bound_Ptr<Sequence, ACE_Null_Mutex> SequencePtr;

class CompareSequenceNumbers
{
public:
    int operator() (const SequencePtr & s1, const SequencePtr & s2)
    {
        int s1_number = ACE_OS::atoi(s1->Start().Value());
        int s2_number = ACE_OS::atoi(s2->Start().Value());

        return s1_number < s2_number;
    }
};


#endif //#ifndef Sequence_h
