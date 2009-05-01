/*
 * $Id: package_utils.h,v 1.1 2009/05/01 13:34:06 john_f Exp $
 *
 * Package-related utility functions
 *
 * Copyright (C) 2009 British Broadcasting Corporation
 * All rights reserved
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

#ifndef package_utils_h
#define package_utils_h

#include "DataTypes.h"
#include "Package.h"
#include "MCClipDef.h"

namespace prodauto
{

// get the startTime in the file SourcePackage referenced by the materialPackage
int64_t getStartTime(prodauto::MaterialPackage * materialPackage, prodauto::PackageSet & packages, prodauto::Rational editRate);
void getStartAndEndTimes(MaterialPackage * materialPackage, PackageSet & packages, Rational editRate,
                                int64_t & startTimecode, Date & startDate, int64_t & endTimecode, Date & endDate);
prodauto::Rational getVideoEditRate(prodauto::Package * package);
prodauto::Rational getVideoEditRate(prodauto::Package * package, prodauto::PackageSet & packages);
prodauto::Rational getVideoEditRate(prodauto::MCClipDef * mcClipDef);

int64_t convertLength(prodauto::Rational inRate, int64_t inLength, prodauto::Rational outRate);
uint16_t getTimecodeBase(prodauto::Rational rate);

}; // namespace prodauto

#endif //ifndef package_utils_h

