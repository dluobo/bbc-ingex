/*
 * $Id: IngexPlayer.h,v 1.5 2009/01/29 07:10:26 stuart_hc Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Modifications: Matthew Marks
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

#ifndef __PRODAUTO_INGEX_PLAYER_H__
#define __PRODAUTO_INGEX_PLAYER_H__


#include <string>
#include <vector>

#include <media_player.h>

#include "IngexPlayerListener.h"
#include "IngexPlayerException.h"


namespace prodauto
{


class IngexPlayer
{
public:

    virtual ~IngexPlayer() {};


    /* X11 display */

    virtual bool setX11WindowName(std::string name) = 0;


    /* stop the player and free resources */

    virtual bool stop() = 0;


    /* toggle locking out player control functions below, eg. to prevent accidents during playback */

    virtual bool toggleLock() = 0;


    /* audio/video stream control */

    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool togglePlayPause() = 0;
    /* whence = SEEK_SET, SEEK_CUR or SEEK_END */
    /* offset is in 1/1000% if unit equals PERCENTAGE_PLAY_UNIT,
    ie. multiple a percentage position by 1000 to get offset */
    virtual bool seek(int64_t offset, int whence, PlayUnit unit) = 0;
    virtual bool playSpeed(int speed) = 0; /* -1 <= speed >=1 */
    virtual bool step(bool forward) = 0;
    virtual bool muteAudio(int mute /* -1=toggle, 0=not mute, 1=mute */) = 0;


    /* mark-in and mark-out controls */

    /* TODO: add toggle parameters */
    virtual bool mark(int type) = 0;
    virtual bool markPosition(int64_t position, int type) = 0;
    virtual bool clearMark() = 0;
    virtual bool clearAllMarks() = 0;
    virtual bool seekNextMark() = 0;
    virtual bool seekPrevMark() = 0;


    /* on screen timecode display */

    virtual bool setOSDScreen(OSDScreen screen) = 0;
    virtual bool nextOSDScreen() = 0;
    virtual bool setOSDTimecode(int index, int type, int subType) = 0;
    virtual bool nextOSDTimecode() = 0;


    /* video switch control */

    virtual bool switchNextVideo() = 0;
    virtual bool switchPrevVideo() = 0;
    virtual bool switchVideo(int index) = 0;


    /* audio switch control */

    virtual bool switchNextAudioGroup() = 0;
    virtual bool switchPrevAudioGroup() = 0;
    virtual bool switchAudioGroup(int index) = 0; /* index == 0 is equiv. to snapAudioToVideo */
    virtual bool snapAudioToVideo() = 0;


    /* review */

    virtual bool reviewStart(int64_t duration) = 0;
    virtual bool reviewEnd(int64_t duration) = 0;
    virtual bool review(int64_t duration) = 0;

};


};



#endif
