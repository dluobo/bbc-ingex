/*
 * $Id: MXFWriterException.h,v 1.1 2007/09/11 14:08:44 stuart_hc Exp $
 *
 * MXF writer exception
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef __PRODAUTO_MXFWRITEREXCEPTION_H__
#define __PRODAUTO_MXFWRITEREXCEPTION_H__


#include <ProdAutoException.h>


namespace prodauto
{

class MXFWriterException : public ProdAutoException
{
public:
    MXFWriterException(const char* format, ...);
    virtual ~MXFWriterException();
};


};


#endif

