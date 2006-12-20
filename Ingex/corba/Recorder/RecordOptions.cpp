/*
 * $Id: RecordOptions.cpp,v 1.1 2006/12/20 12:28:26 john_f Exp $
 *
 * Class for card-specific (i.e. thread-specific) recording data.
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

#include "RecordOptions.h"

RecordOptions::RecordOptions()
 : enabled(false), mFramesWritten(0), mFramesDropped(0) , mTargetDuration(0)
{
    for(int i = 0; i < 5; ++i)
    {
        track_enable[i] = false;
    }
}
