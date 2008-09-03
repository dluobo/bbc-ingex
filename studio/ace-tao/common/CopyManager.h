/*
 * $Id: CopyManager.h,v 1.3 2008/09/03 13:43:33 john_f Exp $
 *
 * Class to manage file copying in a separate process.
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

#ifndef CopyManager_h
#define CopyManager_h

#include <ace/Process_Manager.h>
#include <string>

namespace CopyMode
{
    enum EnumType { OLD, NEW };
}

class CopyManager
{
public:
    CopyManager(CopyMode::EnumType mode = CopyMode::OLD);
    void Command(const std::string & command);
    void RecorderName(const std::string & s);
    void ClearSrcDest();
    void AddSrcDest(const std::string & src, const std::string & dest);
    void StartCopying(unsigned int index);
    void StopCopying(unsigned int index);

private:
    void StopProcess();

    CopyMode::EnumType mMode;
    bool mEnabled;
	pid_t mPid;
    ACE_Process_Manager * mPm;
    ACE_Process_Options mOptions;
    std::string mCommand;
    std::string mRecorderName;
    std::string mArgs;
};

#endif //#ifndef CopyManager_h

