/*
 * $Id: CodedFrameBuffer.h,v 1.1 2009/06/12 17:41:02 john_f Exp $
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

#ifndef CodedFrameBuffer_h
#define CodedFrameBuffer_h

#include <map>
#include <ace/Thread_Mutex.h>

class CodedFrame
{
public:
    CodedFrame(const void * data, size_t size, bool err);
    ~CodedFrame();
    void * Data() { return mData; }
    size_t Size() { return mSize; }
    bool Error() { return mError; }
private:
    void * mData;
    size_t mSize;
    bool mError;
};


class CodedFrameBuffer
{
public:
    ~CodedFrameBuffer();
    void QueueFrame(void * data, size_t size, int index, bool err = false);
    CodedFrame * GetFrame(int index);
    unsigned int QueueSize();
private:
    std::map<int, CodedFrame *> mFrameBuffer;
    ACE_Thread_Mutex mFrameBufferMutex;
};

#endif // ifndef CodedFrameBuffer_h
