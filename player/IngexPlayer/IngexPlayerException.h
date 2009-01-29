/*
 * $Id: IngexPlayerException.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __PRODAUTO_INGEX_PLAYER_EXCEPTION_H__
#define __PRODAUTO_INGEX_PLAYER_EXCEPTION_H__


#include <string>


namespace prodauto
{


class IngexPlayerException
{
public:
    IngexPlayerException();
    IngexPlayerException(const char* format, ...);
    virtual ~IngexPlayerException();

    std::string getMessage() const;

protected:
    std::string _message;
};


};



#endif
