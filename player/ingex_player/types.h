/*
 * $Id: types.h,v 1.4 2008/10/29 17:47:42 john_f Exp $
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

#ifndef __TYPES_H__
#define __TYPES_H__



typedef struct
{
    const char* input;
    const char* description;
} ControlInputHelp;


typedef enum
{
    WHITE_COLOUR = 0,
    LIGHT_WHITE_COLOUR,
    LIGHT_GREY_COLOUR,
    YELLOW_COLOUR,
    CYAN_COLOUR,
    GREEN_COLOUR,
    MAGENTA_COLOUR,
    RED_COLOUR,
    BLUE_COLOUR,
    BLACK_COLOUR,
    ORANGE_COLOUR
} Colour;

typedef struct 
{
    unsigned char Y;
    unsigned char U;
    unsigned char V;
} YUVColours;

static const YUVColours g_rec601YUVColours[] = 
{
    {235, 128, 128},    /* WHITE_COLOUR */
    {210, 128, 128},    /* LIGHT_WHITE_COLOUR */
    {190, 128, 128},    /* LIGHT_GREY_COLOUR */
    {210, 16, 146},     /* YELLOW_COLOUR */
    {169, 166, 16},     /* CYAN_COLOUR */
    {144, 53, 34},      /* GREEN_COLOUR */
    {106, 200, 221},    /* MAGENTA_COLOUR */
    {81, 90, 240},      /* RED_COLOUR */
    {40, 240, 110},     /* BLUE_COLOUR */
    {16, 128, 128},     /* BLACK_COLOUR */
    {165, 43, 180},     /* ORANGE_COLOUR */
};

typedef struct
{
    int num;
    int den;
} Rational;

static const Rational g_palFrameRate = {25, 1};
static const Rational g_profAudioSamplingRate = {48000, 1};

typedef struct
{
    int isDropFrame;
    unsigned int hour;
    unsigned int min;
    unsigned int sec;
    unsigned int frame;
} Timecode;

static const Timecode g_invalidTimecode = {0, 99, 99, 99, 99};


typedef struct
{
    int type;
    char name[32];
    Colour colour;
} MarkConfig;

typedef struct
{
    MarkConfig configs[32];
    int numConfigs;
} MarkConfigs;



#endif


