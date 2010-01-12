/*
 * $Id: digibeta_dropout.h,v 1.1 2010/01/12 16:02:06 john_f Exp $
 *
 * DigiBeta dropout detector
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Jim Easterbrook
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

#ifndef __DIGIBETA_DROPOUT__
#define __DIGIBETA_DROPOUT__

#include "YUV_frame.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int		strength;
	int		sixth, seq;
} dropout_result;

// Analyse a frame to detect DigiBeat dropout blocks
// Returns a measure of the strength of the worst dropout in the frame
dropout_result digibeta_dropout(YUV_frame* in_frame, int xOffset, int yOffset,
								int* workSpace);

#ifdef __cplusplus
}
#endif

#endif // __DIGIBETA_DROPOUT__

