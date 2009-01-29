/*
 * $Id: tc_overlay.h,v 1.4 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * Create burnt-in timecode.
 *
 * Copyright (C) 2007 British Broadcasting Corporation
 * All rights reserved
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

#ifndef  tc_overlay_h
#define tc_overlay_h

#include "integer_types.h"

typedef void tc_overlay_t;
typedef enum { TC420, TC422 } tc_pix_fmt_t ;

#ifdef __cplusplus
extern "C" {
#endif

extern tc_overlay_t * tc_overlay_init();
extern void tc_overlay_close(tc_overlay_t * in_tco);
extern void tc_overlay_setup(tc_overlay_t * in_tco, int frame_number);
extern void tc_overlay_apply(tc_overlay_t * in_tco,
                                uint8_t * y_comp, uint8_t * u_comp, uint8_t * v_comp,
                                unsigned int width, unsigned int height,
                                int tc_xoffset, unsigned int tc_yoffset,
                                tc_pix_fmt_t pix_fmt);

#ifdef __cplusplus
}
#endif

#endif // #ifndef tc_overlay
