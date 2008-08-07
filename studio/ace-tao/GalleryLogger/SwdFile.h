/*
 * $Id: SwdFile.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to read ScriptWriter II files.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
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
    
#ifndef SwdFile_h
#define swdFile_h


//#include <iostream>
//#include <iomanip>
#include <vector>
//using namespace std;

//#include <stdlib.h>
//#include <stdio.h>

//#include <errno.h>
//#include <string.h>





// Determine host operating system.
//
#if defined(_WIN32)
#define OM_OS_WINDOWS
#elif defined(_MAC) || defined(macintosh)
#define OM_OS_MACOS
#elif defined(__MWERKS__) && defined(__MACH__)
#define OM_OS_MACOSX
#elif defined(__sgi) || defined(__linux__) || defined (__FreeBSD__) || \
      defined (__APPLE__) || defined(__CYGWIN__)
#define OM_OS_UNIX
#elif defined (sun)
#define OM_OS_SOLARIS
#else
#error "Can't determine host operating system"
#endif

// Determine which implementation of structured storage to use.
//
#if defined(OM_OS_WINDOWS)
#define OM_USE_WINDOWS_SS
#elif defined(OM_OS_MACOS)
#define OM_USE_MACINTOSH_SS
#elif defined(OM_OS_MACOSX)
#define OM_USE_WRAPPED_MACINTOSH_SS
#elif defined(OM_OS_UNIX)
#define OM_USE_REFERENCE_SS
#elif defined(OM_OS_SOLARIS)
#warning  "not supported on Solaris"

#else
#error "Don't know which implementation of structured storage to use."
#endif

// Include the structured storage headers. These are different
// depending on the implementation.
//
#if defined(OM_USE_WINDOWS_SS)
#include <objbase.h>
#elif defined(OM_USE_MACINTOSH_SS)
#include "macpub.h"
#include "wintypes.h"
#include "macdef.h"
#include "compobj.h"
#include "storage.h"
#include "initguid.h"
#include "coguid.h"
#elif defined(OM_USE_WRAPPED_MACINTOSH_SS)
#include "macpub.h"
#include "wintypes.h"
#include "macdef.h"
#include "compobj.h"
#include "storage.h"
#include "initguid.h"
#include "coguid.h"
#include "SSWrapper.h"
#elif defined(OM_USE_REFERENCE_SS)
#include "h/storage.h"
#endif



// Determine whether or not UNICODE versions of the APIs are in use.
//
#if defined(OM_OS_WINDOWS) && defined(UNICODE)
#define OM_UNICODE_APIS
#endif

// OMCHAR is used for all character arguments to functions whose
// prototype changes when UNICODE is defined.
//
#if defined(OM_UNICODE_APIS)
typedef wchar_t OMCHAR;
#else
typedef char OMCHAR;
#endif

// Console input for Macintosh (two different methods).
//
#if defined(OM_OS_MACOS)
#if !defined(USE_DATAINPUT)
#include <console.h>
#else
#include "DataInput.h"
#endif
#endif

typedef unsigned short int OMUInt16;

typedef OMUInt16 OMCharacter;




class SwdFile 
{
public:
    SwdFile (void);

    struct Seq
    {
        std::string section;
        std::string number;
        std::string name;
    };
    //vector <string> OrderedSequence; // containing ordered data
    std::vector <Seq> OrderedSequence; // containing ordered data
    char* baseName(char* fullName);
    size_t warningCount;
    size_t errorCount;
    void SsfFile (const char* fileName);
    std::string skt; // ordermode of file
   

private:

    void ReadStorage(IStorage* storage,STATSTG* statstg,
                     char* pathName,int isRoot);
    void openStorage(const char* fileName, IStorage** storage);
    //void StoreOrder(string slt);
    void FindOrder(IStream* stream, STATSTG* statstg, char* pathName);
    void StoreData(IStream* stream, STATSTG* statstg, char* pathName);
    char map(int c);
    static char table[128];
    void ReadStream (IStream* stream, STATSTG* statstg, char* pathName, std::string &str );

    void fatalError(char* routineName, char* message);
    void error(char* routineName, char* message);
    void warning(char* routineName, char* message);
    int checkStatus(const char* fileName, DWORD resultCode);
    #if defined(OM_UNICODE_APIS)
    void convert(wchar_t* wcName, size_t length, const char* name);
    void convert(char* cName, size_t length, const wchar_t* name);
    #else
    void convert(char* cName, size_t length, const OMCharacter* name);
    #endif
    void convert(char* cName, size_t length, const char* name);
    void convertName(char* cName,
                            size_t length,
                            OMCHAR* wideName,
                            char** tag);
    void resetStatistics(void);
    void GetOrderedData(void);
    static void initializeCOM(void);
    static void finalizeCOM(void);
  



    bool verboseFlag;
    // Statistics gathering
    //
    size_t totalStorages;
    size_t totalStreams;

    size_t totalPropertyBytes;
    size_t totalObjects;
    size_t totalProperties;

    size_t totalStreamBytes;
    size_t totalFileBytes;

    // Validity checking

    std::vector <Seq> SeqList;      //// containing unordered data
    std::vector <std::string> OrderList;  // containing sequence order


};

#endif // #ifndef SwdFile_h
