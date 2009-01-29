/*
 * $Id: test_emulate_key.c,v 1.3 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/keysym.h>


#include "emulate_key.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }



int main (int argc, const char** argv)
{
    EmulateKey* emu;


    CHECK_FATAL(create_emu(&emu));

    /* 'a' */
    CHECK_FATAL(emu_key(emu, XK_a, 0));

    /* '+' becomes '=' because no shift modifier */
    CHECK_FATAL(emu_key(emu, '+', 0));

    /* '+' ok with shift modifier */
    CHECK_FATAL(emu_key(emu, '+', ShiftMask));

    /* '1' */
    CHECK_FATAL(emu_key_down(emu, '1', 0));
    CHECK_FATAL(emu_key_up(emu, '1', 0));

    /* '3' x 3 */
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_down(emu, '3', 0));
    CHECK_FATAL(emu_key_up(emu, '3', 0));

    free_emu(&emu);


    return 0;
}


