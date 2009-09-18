/*
 * $Id: Utilities.h,v 1.2 2009/09/18 16:50:11 philipn Exp $
 *
 * General utilities
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 
#ifndef __PRODAUTO_UTILITIES_H__
#define __PRODAUTO_UTILITIES_H__


#include <vector>
#include <string>

#include "DataTypes.h"


namespace prodauto
{

 
UMID generateUMID();
UMID generateLegacyUMID();
UMID getUMID(std::string umidStr);
std::string getUMIDString(UMID umid);

Timestamp generateTimestampNow();
Timestamp generateTimestampStartToday();
Timestamp generateTimestampStartTomorrow();
Timestamp getTimestampFromODBC(std::string timestampFromODBC);
std::string getTimestampString(Timestamp timestamp);

Date generateDateNow();
std::string getDateString(Date date);

std::string getIntervalString(Interval interval);

std::vector<std::string> getScriptReferences(std::string refsString);
std::string getScriptReferencesString(std::vector<std::string> refs);

// returns the file name given a full path
std::string getFilename(std::string filePath);


// used to ensure Element pointers are freed if the vectors goes out of scope
template <class Element>
class VectorGuard
{
public:
    typedef typename std::vector<Element*> VecType;
    typedef typename VecType::const_iterator VecTypeConstIter;
    
    VectorGuard() {}
    VectorGuard(const VecType& vec) 
    {
        _vec = vec;
    }
    
    ~VectorGuard()
    {
        VecTypeConstIter iter;
        for (iter = _vec.begin(); iter != _vec.end(); iter++)
        {
            delete *iter;
        }
    }
    
    VecType& get()
    {
        return _vec;
    }
    
    VecType release()
    {
        VecType ret = _vec;
        _vec.clear();
        
        return ret;
    }
    
private:
    VecType _vec;
};


};







#endif



