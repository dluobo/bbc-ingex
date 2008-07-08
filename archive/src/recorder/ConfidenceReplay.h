/*
 * $Id: ConfidenceReplay.h,v 1.1 2008/07/08 16:25:41 philipn Exp $
 *
 * Runs and provides access to a player running in a separate process
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
#ifndef __RECORDER_CONF_REPLAY_H__
#define __RECORDER_CONF_REPLAY_H__


#include <string>

#include "Threads.h"


#define ITEM_START_MARK         0x00000100


namespace rec
{



class ConfidenceReplay : public Thread
{
public:
    ConfidenceReplay(std::string filename, int httpAccessPort, int64_t startPosition, bool showSecondMarkBar);
    virtual ~ConfidenceReplay();
    
    void forwardControl(std::string command);
    
    void setMark(int64_t position, int type);
    void clearMark(int64_t position, int type);
    void seek(int64_t position);
    void play();
    void seekPositionOrPlay(int64_t position);
    
    int64_t getDuration();
    int64_t getPosition();
    std::string getFilename();
    
private:
    std::string _filename;
    Thread* _stateMonitor;
    
};




};




#endif

