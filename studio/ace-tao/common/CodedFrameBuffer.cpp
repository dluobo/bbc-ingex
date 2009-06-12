/*
 * $Id: CodedFrameBuffer.cpp,v 1.1 2009/06/12 17:41:02 john_f Exp $
 *
 * Buffer to handle coded video frames from multiple encoding
 * threads.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include <cstring>
#include <ace/Log_Msg.h>
#include <ace/Guard_T.h>

#include "CodedFrameBuffer.h"

// CodedFrame class

CodedFrame::CodedFrame(const void * data, size_t size, bool err)
: mSize(size), mError(err)
{
    mData = malloc(size);
    memcpy(mData, data, size);
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("Created coded frame of size %u\n"), size));
}

CodedFrame::~CodedFrame()
{
    //ACE_DEBUG((LM_DEBUG, ACE_TEXT("CodedFrame destructor freeing data\n")));
    free(mData);
}

// CodedFrameBuffer class

CodedFrameBuffer::~CodedFrameBuffer()
{
    for (std::map<int, CodedFrame *>::iterator
        it = mFrameBuffer.begin(); it != mFrameBuffer.end(); ++it)
    {
        CodedFrame * cf = it->second;
        delete cf;
    }
}

void CodedFrameBuffer::QueueFrame(void * data, size_t size, int index, bool err)
{
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Queueing coded frame %d\n"), index));
    CodedFrame * p_coded_frame = new CodedFrame(data, size, err);
    {
        ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

        mFrameBuffer[index] = p_coded_frame;
    }
}

CodedFrame * CodedFrameBuffer::GetFrame(int index)
{
    CodedFrame * cf = 0;
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);
    std::map<int, CodedFrame *>::iterator it;
    if (mFrameBuffer.end() != (it = mFrameBuffer.find(index)))
    {
        cf = it->second;
        mFrameBuffer.erase(it);
    }

    return cf;
}

unsigned int CodedFrameBuffer::QueueSize()
{
    ACE_Guard<ACE_Thread_Mutex> guard(mFrameBufferMutex);

    return mFrameBuffer.size();
}

