/*
 * $Id: Macros.h,v 1.2 2008/10/29 17:49:55 john_f Exp $
 *
 *
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

#ifndef __PRODAUTO_MACROS_H__
#define __PRODAUTO_MACROS_H__


#include <macros.h>


#define THROW_EXCEPTION(err) \
    ml_log_error err; \
    ml_log_error("  near %s:%d\n", __FILE__, __LINE__); \
    throw IngexPlayerException err;


#define CHK_OTHROW(cmd) \
    if (!(cmd)) \
    { \
        THROW_EXCEPTION(("'%s' failed\n", #cmd)); \
    }
    
#define CHK_OTHROW_MSG(cmd, err) \
    if (!(cmd)) \
    { \
        THROW_EXCEPTION(err); \
    }
    
    
#define SAFE_DELETE(var) \
    delete *var; \
    *var = 0;

#define SAFE_ARRAY_DELETE(var) \
    delete [] *var; \
    *var = 0;

    
#endif


