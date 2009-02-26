/*
 * $Id: DataTypes.h,v 1.3 2009/02/26 19:39:29 john_f Exp $
 *
 * General data types
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
 
#ifndef __PRODAUTO_DATATYPES_H__
#define __PRODAUTO_DATATYPES_H__

#include <cstring>


#if defined(_MSC_VER) && defined(_WIN32)

// if inttypes are provided elsewhere in your code, then set this define
#if !defined(INTTYPES_DEFINED)
#define INTTYPES_DEFINED

// Provide ISO C types which are missing in MSVC
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef __int64 int64_t;

#endif

#else

// include ISO fixed-width integer types
#include <inttypes.h>

#endif



namespace prodauto
{

typedef struct
{
    int16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t qmsec;
} Timestamp;

static const Timestamp g_nullTimestamp = {0, 0, 0, 0, 0, 0, 0};

typedef Timestamp Interval;


inline bool operator == (const Timestamp& l, const Timestamp& r)
{
    return memcmp(&l, &r, sizeof(Timestamp)) == 0;
}

inline bool operator != (const Timestamp& l, const Timestamp& r)
{
    return memcmp(&l, &r, sizeof(Timestamp)) != 0;
}


typedef struct
{
    int16_t year;
    uint8_t month;
    uint8_t day;
} Date;

static const Date g_nullDate = {0, 0, 0};


inline bool operator == (const Date& l, const Date& r)
{
    return memcmp(&l, &r, sizeof(Date)) == 0;
}

inline bool operator != (const Date& l, const Date& r)
{
    return memcmp(&l, &r, sizeof(Date)) != 0;
}



typedef struct 
{
    uint8_t octet0;
    uint8_t octet1;
    uint8_t octet2;
    uint8_t octet3;
    uint8_t octet4;
    uint8_t octet5;
    uint8_t octet6;
    uint8_t octet7;
    uint8_t octet8;
    uint8_t octet9;
    uint8_t octet10;
    uint8_t octet11;
    uint8_t octet12;
    uint8_t octet13;
    uint8_t octet14;
    uint8_t octet15;
    uint8_t octet16;
    uint8_t octet17;
    uint8_t octet18;
    uint8_t octet19;
    uint8_t octet20;
    uint8_t octet21;
    uint8_t octet22;
    uint8_t octet23;
    uint8_t octet24;
    uint8_t octet25;
    uint8_t octet26;
    uint8_t octet27;
    uint8_t octet28;
    uint8_t octet29;
    uint8_t octet30;
    uint8_t octet31;
} UMID;

static const UMID g_nullUMID = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


inline bool operator == (const UMID& l, const UMID& r)
{
    return memcmp(&l, &r, sizeof(UMID)) == 0;
}

inline bool operator != (const UMID& l, const UMID& r)
{
    return memcmp(&l, &r, sizeof(UMID)) != 0;
}

inline bool operator < (const UMID& l, const UMID& r)
{
    return memcmp(&l, &r, sizeof(UMID)) < 0;
}



struct Rational
{
    Rational() : numerator(0), denominator(0) {}
    Rational(int32_t num, int32_t den) : numerator(num), denominator(den) {}
    int32_t numerator;
    int32_t denominator;
};

inline bool operator == (const Rational& l, const Rational& r)
{
    return l.numerator == r.numerator && l.denominator == r.denominator;
}

inline bool operator != (const Rational& l, const Rational& r)
{
    return l.numerator != r.numerator || l.denominator != r.denominator;
}

const Rational g_nullRational (0, 0);

const Rational g_palEditRate (25, 1);
const Rational g_ntscEditRate (30000, 1001);
const Rational g_audioEditRate (48000, 1);

const Rational g_4x3ImageAspect (4, 3);
const Rational g_16x9ImageAspect (16, 9);

};


#endif


