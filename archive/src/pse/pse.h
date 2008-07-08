/*
 * $Id: pse.h,v 1.1 2008/07/08 16:24:58 philipn Exp $
 *
 * Interface definition of a class implementing PSE analysis
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

/*
    Defines the interface of classes implementing PSE analysis. The input is a 
    720x576 UYVY video frame. The results are the positions and analysis values
    (red flash, luminance flash, spatial patterns and extended warning) of 
    frames with non-zero analysis values.
*/


#ifndef pse_h
#define pse_h

#include <inttypes.h>
#include <vector>

typedef struct
{
    uint64_t position;
    int16_t red;
    int16_t flash;
    int16_t spatial;
    bool extended;
} PSEResult;

class PSE_Analyse
{
public:
    PSE_Analyse(){};
    virtual ~PSE_Analyse(){};

    virtual bool init(void) = 0;
    virtual bool fini(void) = 0;
    virtual bool open(void) = 0;
    virtual bool close(void) = 0;

    virtual bool analyse_frame(const uint8_t *video_frame) = 0;
    virtual bool get_remaining_results(std::vector<PSEResult> &p_results) = 0;
};

#endif // pse_h
