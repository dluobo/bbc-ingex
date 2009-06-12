/*
 * $Id: ShmTimecodeReader.h,v 1.1 2009/06/12 17:55:49 john_f Exp $
 *
 * Timecode reader using Ingex shared memory as source.
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

#ifndef ShmTimecodeReader_h
#define ShmTimecodeReader_h

#include "TimecodeReader.h"

class ShmTimecodeReader : public TimecodeReader
{
public:
    ShmTimecodeReader();
    enum TcEnum { LTC, VITC };
    void TcMode(TcEnum tc_mode);
    std::string Timecode();
};

#endif //#ifndef ShmTimecodeReader_h

