/*
 * $Id: pse_simple.cpp,v 1.1 2008/07/08 16:24:57 philipn Exp $
 *
 * Simple PSE analysis (currently does no analysis and always passes)
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

#include "pse_simple.h"

PSE_Simple::PSE_Simple(void)
{
    initialised = false;
    opened = false;
}

PSE_Simple::~PSE_Simple(void)
{
}

bool PSE_Simple::init(void)
{
    initialised = true;
    return true;
}

bool PSE_Simple::fini(void)
{
    initialised = false;
    return true;
}

bool PSE_Simple::open(void)
{
    // Clear frame and results counters and variables
    FrameCounter = 0;
    ResultsCounter = 0;
    results.clear();

    opened = true;
    return true;
}

bool PSE_Simple::analyse_frame(const uint8_t *video_frame)
{
    // Initialise and open engine if not already
    if (! initialised)
        if (! init())
            return false;

    if (! opened)
        if (! open())
            return false;

    FrameCounter++;
    ResultsCounter++;

    return true;
}

bool PSE_Simple::get_remaining_results(std::vector<PSEResult> &all_results)
{
    all_results = results;
    return true;
}

bool PSE_Simple::close(void)
{
    opened = false;
    return true;
}
