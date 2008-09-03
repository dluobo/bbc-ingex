/*
 * $Id: IdlEnumText.h,v 1.2 2008/09/03 13:43:34 john_f Exp $
 *
 * Extended ACE_Data_Block to carry arbitrary data.
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

#ifndef IdlEnumText_h
#define IdlEnumText_h

#include "DataSourceC.h"

/**
Conversion of IDL Enum types to character strings.
Typically used in output to debug logfiles.
*/
class IdlEnumText
{
public:
    static const char * ConnectionState(ProdAuto::DataSource::ConnectionState s);
};

#endif // #ifndef IdlEnumText_h

