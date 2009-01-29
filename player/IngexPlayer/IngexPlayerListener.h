/*
 * $Id: IngexPlayerListener.h,v 1.5 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __PRODAUTO_INGEX_PLAYER_LISTENER_H__
#define __PRODAUTO_INGEX_PLAYER_LISTENER_H__


#include <string>
#include <vector>

#include <media_player.h>
#include <x11_common.h>

namespace prodauto
{

class ReadWriteLockGuard
{
public:
    ReadWriteLockGuard(pthread_rwlock_t* rwlock, bool writeLock);
    ~ReadWriteLockGuard();

private:
    pthread_rwlock_t* _rwlock;
};

class IngexPlayerListener;

class IngexPlayerListenerRegistry
{
public:
    IngexPlayerListenerRegistry();
    ~IngexPlayerListenerRegistry();
    bool registerListener(IngexPlayerListener* listener);
    bool unregisterListener(IngexPlayerListener* listener);
    /* allowing access for C callback functions */
    pthread_rwlock_t _listenersRWLock;
    std::vector<IngexPlayerListener*> _listeners;
};

class IngexPlayerListener
{
public:
    IngexPlayerListener(IngexPlayerListenerRegistry* registry);
    virtual ~IngexPlayerListener();

    virtual IngexPlayerListenerRegistry* getRegistry();
    virtual void setRegistry(IngexPlayerListenerRegistry* registry);
    virtual void unsetRegistry();

    virtual void frameDisplayedEvent(const FrameInfo* frameInfo) = 0;
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo) = 0;
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event) = 0;
    virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo) = 0;
    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo) = 0;
    virtual void playerCloseRequested() = 0;
    virtual void playerClosed() = 0;
    virtual void keyPressed(int key, int modifier) = 0;
    virtual void keyReleased(int key, int modifier) = 0;
    virtual void progressBarPositionSet(float position) = 0;
    virtual void mouseClicked(int imageWidth, int imageHeight, int xPos, int yPos) = 0;

protected:
    IngexPlayerListenerRegistry* _registry;
};


};



#endif
