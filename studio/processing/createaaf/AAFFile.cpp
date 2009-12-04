/*
 * $Id: AAFFile.cpp,v 1.9 2009/12/04 18:30:16 john_f Exp $
 *
 * AAF file for defining clips, multi-camera clips, etc
 *
 * Copyright (C) 2006, BBC, Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <cassert>
#include <cstdlib>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <AAFResult.h>
#include <AAFClassDefUIDs.h>
#include <AAFFileKinds.h>
#include <AAFSmartPointer.h>
#include <AAFSmartPointer2.h>
#include <AAFDataDefs.h>
#include <AAFStoredObjectIDs.h>
#include <AAFCompressionDefs.h>
#include <AAFContainerDefs.h>
#include <AAFTypeDefUIDs.h>
#include <AAFExtEnum.h>


#include "AAFFile.h"
#include "CreateAAFException.h"
#include "package_utils.h"

#include <DatabaseEnums.h>
#include <Logging.h>



using namespace std;
using namespace prodauto;


const bool DEBUG_PRINT = false;

#define AAF_CHECK(cmd) \
{ \
    HRESULT result = cmd; \
    if (!AAFRESULT_SUCCEEDED(result)) \
    { \
        PA_LOGTHROW(CreateAAFException, ("'%s' failed (result = 0x%x)", #cmd, result)); \
    } \
}

    
typedef struct
{
    aafUInt16 red;
    aafUInt16 green;
    aafUInt16 blue;
} RGBColor;


// simple helper class to initialize and cleanup AAF library.
struct CAAFInitialize
{
    CAAFInitialize(const char *dllname = NULL)
    {
        HRESULT hr = AAFLoad(dllname);
        if (!AAFRESULT_SUCCEEDED(hr)) 
        {
            PA_LOGTHROW(CreateAAFException, ("Failed to load the AAF library"));
        }
    }

    ~CAAFInitialize()
    {
        AAFUnload();
    }
};

CAAFInitialize aafInit;
    

const aafUID_t NIL_UID = {0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0}};

//
// AAF Extensions required to interoperate with Avid editors
//

// Type defs
static const aafUID_t kAAFTypeID_RGBColor = 
    {0xe96e6d43, 0xc383, 0x11d3, {0xa0, 0x69, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFTypeID_AvidStrongReference = 
    {0xf9a74d0a, 0x7b30, 0x11d3, {0xa0, 0x44, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kTaggedValueStrongReferenceTypeID =
    {0xda60bb00, 0xba41, 0x11d4, {0xa8, 0x12, 0x8e, 0x54, 0x1b, 0x97, 0x2e, 0xa3}};

// Class defs
static const aafUID_t kAAFClassID_Avid_MC_Mob_Reference =
    {0x6619f8e0, 0xfe77, 0x11d3, {0xa0, 0x84, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};

// Property defs
static const aafUID_t kAAFPropID_DIDResolutionID = 
    {0xce2aca4d, 0x51ab, 0x11d3, {0xa0, 0x24, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_DIDFrameSampleSize = 
    {0xce2aca50, 0x51ab, 0x11d3, {0xa0, 0x24, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_DIDImageSize = 
    {0xce2aca4f, 0x51ab, 0x11d3, {0xa0, 0x24, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};

static const aafUID_t kAAFPropID_CommentMarkerColor = 
    {0xe96e6d44, 0xc383, 0x11d3, {0xa0, 0x69, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};

// AppCode and MobAttributeList are used by Avid to represent MultiCam groups
static const aafUID_t kAAFPropID_MobAppCode = 
    {0x96c46992, 0x4f62, 0x11d3, {0xa0, 0x22, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_MobAttributeList = 
    {0x60958183, 0x47b1, 0x11d4, {0xa0, 0x1c, 0x00, 0x04, 0xac, 0x96, 0x9f, 0x50}};
static const aafUID_t kAAFPropID_PortableObject = 
    {0xb6bb5f4e, 0x7b37, 0x11d3, {0xa0, 0x44, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_PortableObjectClassID = 
    {0x08835f4f, 0x7b28, 0x11d3, {0xa0, 0x44, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_Mob_Reference_MobID = 
    {0x81110e9f, 0xfe7c, 0x11d3, {0xa0, 0x84, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
static const aafUID_t kAAFPropID_Mob_Reference_Position = 
    {0x81110ea0, 0xfe7c, 0x11d3, {0xa0, 0x84, 0x00, 0x60, 0x94, 0xeb, 0x75, 0xcb}};
    
// Compression defs
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_21 =
    {0x0e040201, 0x0201, 0x0108, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_31 =
    {0x0e040201, 0x0201, 0x0106, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_101 =
    {0x0e040201, 0x0201, 0x0104, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_101m =
    {0x0e040201, 0x0201, 0x0402, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_151s =
    {0x0e040201, 0x0201, 0x0202, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_Avid_MJPEG_201 =
    {0x0e040201, 0x0201, 0x0102, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_IMX_30 =
    {0x04010202, 0x0102, 0x0105, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_IMX_40 =
    {0x04010202, 0x0102, 0x0103, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_IMX_50 =
    {0x04010202, 0x0102, 0x0101, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
static const aafUID_t kAAFCompressionDef_DNxHD =
    {0x0e040201, 0x0204, 0x0100, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};
// kAAFCompressionDef_DV_Based_100Mbps_1080x50I was added to the AAF SDK on May 14 2009
// and we duplicate the constant here to support older SDKs
static const aafUID_t kAAFCompressionDef_DV_Based_100Mbps_1080x50I_INGEX =
    {0x04010202, 0x0202, 0x0600, {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

static const RGBColor g_rgbColors[] =
{
    {65534, 65535, 65535}, // white
    {41471, 12134, 6564 }, // red
    {58981, 58981, 6553 }, // yellow
    {13107, 52428, 13107}, // green
    {13107, 52428, 52428}, // cyan
    {13107, 13107, 52428}, // blue
    {52428, 13107, 52428}, // magenta
    {0    , 0    , 0    }  // black
};



static aafMobID_t convertUMID(prodauto::UMID& umid)
{
    aafMobID_t mobID;
    
    memcpy(&mobID.SMPTELabel, &umid, 12);
    mobID.length = umid.octet12;
    mobID.instanceHigh = umid.octet13;
    mobID.instanceMid = umid.octet14;
    mobID.instanceLow = umid.octet15;

    // TODO: this assumes an AAF SDK UMID
    mobID.material.Data1 = (umid.octet16 << 24) | (umid.octet17 << 16) | (umid.octet18 << 8) | (umid.octet19);
    mobID.material.Data2 = (umid.octet20 << 8) | (umid.octet21);
    mobID.material.Data3 = (umid.octet22 << 8) | (umid.octet23);
    memcpy(&mobID.material.Data4, &umid.octet24, 8); 
    
    return mobID;
}

static aafTimeStamp_t convertTimestamp(prodauto::Timestamp& in)
{
    aafTimeStamp_t out;
    
    out.date.year = in.year;
    out.date.month = in.month;
    out.date.day = in.day;
    out.time.hour = in.hour;
    out.time.minute = in.min;
    out.time.second = in.sec;
    out.time.fraction = in.qmsec * 4 / 10; // MXF is 1/250th second and AAF is 1/100th second
    
    return out;
}

static aafRational_t convertRational(prodauto::Rational& in)
{
    aafRational_t out;
    
    out.numerator = in.numerator;
    out.denominator = in.denominator;
    
    return out;
}

static prodauto::Rational convertRational(aafRational_t& in)
{
    prodauto::Rational out;
    
    out.numerator = in.numerator;
    out.denominator = in.denominator;
    
    return out;
}

static wchar_t* getSlotName(bool isPicture, bool isTimecode, int count)
{
    static wchar_t wSlotName[13];
    
    if (isPicture)
    {
        swprintf(wSlotName, 11, L"V%d", count);
    }
    else if (!isTimecode)
    {
        swprintf(wSlotName, 11, L"A%d", count);
    }
    else
    {
        swprintf(wSlotName, 12, L"TC%d", count);
    }
    
    return wSlotName;
}


// auto-deletes wide string
class WStringReturn
{
public:
    WStringReturn() : _value(0) {}
    WStringReturn(wchar_t* value) : _value(value) {}
    ~WStringReturn() { delete [] _value; }
    
    operator wchar_t* () { return _value; }
    
private:
    wchar_t* _value;
};

static WStringReturn convertString(string value)
{
    wchar_t* wValue = new wchar_t[value.size() + 1];

    mbstowcs(wValue, value.c_str(), value.size());
    wValue[value.size()] = L'\0';
    
    return wValue;
}

static WStringReturn createExtendedClipName(string name, int64_t startPosition, Rational editRate)
{
    int timecodeBase = (int)(editRate.numerator / (double)editRate.denominator + 0.5);
    WStringReturn wName(convertString(name));

    size_t size = wcslen(wName) + 33;
    wchar_t* result = new wchar_t[size];
    
    if (startPosition < 0)
    {
        swprintf(result, size, L"%ls.0", (wchar_t*)wName);
    }
    else
    {
        swprintf(result, size, L"%ls.%02d%02d%02d%02d",
            (wchar_t*)wName,
            (int)(startPosition / (60 * 60 * timecodeBase)),
            (int)((startPosition % (60 * 60 * timecodeBase)) / (60 * timecodeBase)),
            (int)(((startPosition % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) / timecodeBase),
            (int)(((startPosition % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) % timecodeBase));
    }
    
    result[size - 1] = L'\0';
    
    return result;
}




AAFFile::AAFFile(string filename, Rational targetEditRate, bool aafxml, bool addAudioEdits)
: _targetEditRate(targetEditRate), _addAudioEdits(addAudioEdits)
{
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDefRecord> pRecordDef;
    IAAFSmartPointer<IAAFTypeDef> pMemberTypeDef;

    remove(filename.c_str());
    wchar_t wFilename[FILENAME_MAX];
    mbstowcs(wFilename, filename.c_str(), FILENAME_MAX);

    aafProductIdentification_t productInfo;
    aafProductVersion_t ver = {0, 1, 0, 0, kAAFVersionUnknown};
    aafCharacter companyName[] = L"BBC Research";
    aafCharacter productName[] = L"Avid MXF import";
    aafCharacter productVersionString[] = L"Unknown";
    productInfo.companyName = companyName;
    productInfo.productName = productName;
    productInfo.productVersion = &ver;
    productInfo.productVersionString = productVersionString;
    productInfo.productID = NIL_UID;
    productInfo.platform = NULL;

    if (aafxml)
    {
        AAF_CHECK(AAFFileOpenNewModifyEx(wFilename, &kAAFFileKind_AafXmlText, 0, &productInfo, &pFile));
    }
    else
    {
        AAF_CHECK(AAFFileOpenNewModifyEx(wFilename, &kAAFFileKind_Aaf4KBinary, 0, &productInfo, &pFile));
    }

    try
    {
        AAF_CHECK(pFile->GetHeader(&pHeader));
    
        // Get the AAF Dictionary from the file
        AAF_CHECK(pHeader->GetDictionary(&pDictionary));
    
        // Lookup class definitions for the objects we want to create
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFMob, &pCDMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFCompositionMob, &pCDCompositionMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFMasterMob, &pCDMasterMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSourceMob, &pCDSourceMob));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSourceClip, &pCDSourceClip));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSequence, &pCDSequence));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFSelector, &pCDSelector));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFFiller, &pCDFiller));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFTapeDescriptor, &pCDTapeDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFCDCIDescriptor, &pCDCDCIDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFDigitalImageDescriptor, &pCDDigitalImageDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFPCMDescriptor, &pCDPCMDescriptor));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFCommentMarker, &pCDCommentMarker));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFDescriptiveMarker, &pCDDescriptiveMarker));
        AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFTimecode, &pCDTimecode));
    
        // Lookup Picture and Sound DataDefinitions
        AAF_CHECK(pDictionary->LookupDataDef(kAAFDataDef_LegacyPicture, &pPictureDef));
        AAF_CHECK(pDictionary->LookupDataDef(kAAFDataDef_LegacySound, &pSoundDef));
        AAF_CHECK(pDictionary->LookupDataDef(kAAFDataDef_DescriptiveMetadata, &pDescriptiveMetadataDef));
        
        // Lookup container definitions
        AAF_CHECK(pDictionary->LookupContainerDef(kAAFContainerDef_AAFKLV, &pAAFKLVContainerDef));
    
        // register extension properties
        AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_Int32, &pTypeDef));
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDResolutionID, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDResolutionID,
                L"ResolutionID", pTypeDef, &pPropertyDef));
        }
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDFrameSampleSize, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDFrameSampleSize,
                L"FrameSampleSize", pTypeDef, &pPropertyDef));
        }
        if (pCDDigitalImageDescriptor->LookupPropertyDef(kAAFPropID_DIDImageSize, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDDigitalImageDescriptor->RegisterOptionalPropertyDef(kAAFPropID_DIDImageSize,
                L"ImageSize", pTypeDef, &pPropertyDef));
        }
        
        if (pCDMob->LookupPropertyDef(kAAFPropID_MobAppCode, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            AAF_CHECK(pCDMob->RegisterOptionalPropertyDef(kAAFPropID_MobAppCode,
                L"AppCode", pTypeDef, &pPropertyDef));
        }
        
        if (pCDCommentMarker->LookupPropertyDef(kAAFPropID_CommentMarkerColor, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            if (pDictionary->LookupTypeDef(kAAFTypeID_RGBColor, &pTypeDef) != AAFRESULT_SUCCESS)
            {
                AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_UInt16, &pMemberTypeDef));
                
                IAAFTypeDef* memberTypes[3];
                memberTypes[0] = pMemberTypeDef;
                memberTypes[1] = pMemberTypeDef;
                memberTypes[2] = pMemberTypeDef;
                
                aafString_t memberNames[3];
                aafCharacter member1Name[] = L"red";
                aafCharacter member2Name[] = L"green";
                aafCharacter member3Name[] = L"blue";
                memberNames[0] = member1Name;
                memberNames[1] = member2Name;
                memberNames[2] = member3Name;
                
                AAF_CHECK(pDictionary->CreateMetaInstance(AUID_AAFTypeDefRecord, IID_IAAFTypeDefRecord, (IUnknown**)&pRecordDef));
                AAF_CHECK(pRecordDef->Initialize(kAAFTypeID_RGBColor, memberTypes, memberNames, 3, L"RGBColor"));
                
                // register members so that a value can be created from a struct
                aafUInt32 offsets[3];
                offsets[0] = offsetof(RGBColor, red);
                offsets[1] = offsetof(RGBColor, green);
                offsets[2] = offsetof(RGBColor, blue);
                
                AAF_CHECK(pRecordDef->RegisterMembers(offsets, 3, sizeof(RGBColor)));

                AAF_CHECK(pRecordDef->QueryInterface(IID_IAAFTypeDef, (void**)&pTypeDef));
                AAF_CHECK(pDictionary->RegisterTypeDef(pTypeDef));
            }
            
            AAF_CHECK(pCDCommentMarker->RegisterOptionalPropertyDef(kAAFPropID_CommentMarkerColor,
                L"CommentMarkerColor", pTypeDef, &pPropertyDef));
        }

        if (pCDMob->LookupPropertyDef(kAAFPropID_MobAttributeList, &pPropertyDef) != AAFRESULT_SUCCESS)
        {
            // Register StrongReference type for containing a TaggedValue
            IAAFSmartPointer<IAAFClassDef> spCDTaggedValue;
            AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFTaggedValue, &spCDTaggedValue));

            IAAFSmartPointer<IAAFTypeDefStrongObjRef> spTaggedValueStrongReference;
            AAF_CHECK(pDictionary->CreateMetaInstance(AUID_AAFTypeDefStrongObjRef, IID_IAAFTypeDefStrongObjRef, (IUnknown **)&spTaggedValueStrongReference));
            AAF_CHECK(spTaggedValueStrongReference->Initialize(kTaggedValueStrongReferenceTypeID, spCDTaggedValue, L"TaggedValueStrongReference"));

            IAAFSmartPointer<IAAFTypeDef> spTD_fa;
            AAF_CHECK(spTaggedValueStrongReference->QueryInterface(IID_IAAFTypeDef, (void**)&spTD_fa));
            AAF_CHECK(pDictionary->RegisterTypeDef(spTD_fa));

            // Register MobAttributeList property of type TaggedValueStrongReferenceVector on class Mob
            AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_TaggedValueStrongReferenceVector, &pTypeDef));
            AAF_CHECK(pCDMob->RegisterOptionalPropertyDef(kAAFPropID_MobAttributeList, L"MobAttributeList", pTypeDef, &pPropertyDef));

            // Register the AvidStrongReference type
            IAAFSmartPointer<IAAFClassDef> cdIntObj;
            IAAFSmartPointer<IAAFTypeDefStrongObjRef> pStrongObjRefDef;
            AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFInterchangeObject, &cdIntObj));
            AAF_CHECK(pDictionary->CreateMetaInstance(AUID_AAFTypeDefStrongObjRef, IID_IAAFTypeDefStrongObjRef, (IUnknown**)&pStrongObjRefDef));
            AAF_CHECK(pStrongObjRefDef->Initialize(kAAFTypeID_AvidStrongReference, cdIntObj, L"AvidStrongReference"));
            AAF_CHECK(pStrongObjRefDef->QueryInterface(IID_IAAFTypeDef, (void**)&pTypeDef));
            AAF_CHECK(pDictionary->RegisterTypeDef(pTypeDef));

            // Register PortableObject property of type AvidStrongReference on class TaggedValue
            AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_AvidStrongReference, &pTypeDef));
            AAF_CHECK(spCDTaggedValue->RegisterOptionalPropertyDef(kAAFPropID_PortableObject, L"PortableObject", pTypeDef, &pPropertyDef));

            // Register PortableObjectClassID property of type UInt32 on class TaggedValue
            AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_UInt32, &pTypeDef));
            AAF_CHECK(spCDTaggedValue->RegisterOptionalPropertyDef(kAAFPropID_PortableObjectClassID, L"PortableObjectClassID", pTypeDef, &pPropertyDef));

            // Add Avid_MC_Mob_Reference class
            IAAFClassDef *pcd = NULL;
            AAF_CHECK(pDictionary->CreateMetaInstance(AUID_AAFClassDef, IID_IAAFClassDef, (IUnknown**)&pcd));
            AAF_CHECK(pcd->Initialize(kAAFClassID_Avid_MC_Mob_Reference, cdIntObj, L"Avid MC Mob Reference", kAAFTrue));

            // Add Mob_Reference_MobID property of type MobIDType
            AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_MobIDType, &pTypeDef));
            AAF_CHECK(pcd->RegisterNewPropertyDef(kAAFPropID_Mob_Reference_MobID, L"Mob Reference MobID", pTypeDef, kAAFFalse, kAAFFalse, &pPropertyDef));

            // Add Mob_Reference_Position property of type Int64
            AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_Int64, &pTypeDef));
            AAF_CHECK(pcd->RegisterNewPropertyDef(kAAFPropID_Mob_Reference_Position, L"Mob Reference Position", pTypeDef, kAAFFalse, kAAFFalse, &pPropertyDef));

            AAF_CHECK(pDictionary->RegisterClassDef(pcd));
        }

    }
    catch (...)
    {    
        if (pFile != 0)
        {
            pFile->Close();
            pFile = IAAFSmartPointer<IAAFFile>(0);
        }
        throw;
    }
}

AAFFile::~AAFFile()
{
    if (pFile != 0)
    {
        pFile->Close();
        pFile = IAAFSmartPointer<IAAFFile>(0);
    }
}

void AAFFile::save()
{
    AAF_CHECK(pFile->Save());
}

void AAFFile::addClip(MaterialPackage* materialPackage, PackageSet& packages)
{
    try
    {
        // create master mob
        mapMasterMob(materialPackage);

        // get packages in material package reference chain
        PackageSet referencedPackages;
        getReferencedPackages(materialPackage, packages, referencedPackages);
        
        // create file and tape source mobs
        PackageSet::const_iterator iter;
        for (iter = referencedPackages.begin(); iter != referencedPackages.end(); iter++)
        {
            Package* package = *iter;
            
            if (package->getType() == MATERIAL_PACKAGE)
            {
                continue;
            }
    
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            
            if (sourcePackage->descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
            {
                mapFileSourceMob(sourcePackage);
            }
            else if (sourcePackage->descriptor->getType() == TAPE_ESSENCE_DESC_TYPE)
            {
                mapTapeSourceMob(sourcePackage);
            }
            else // LIVE_ESSENCE_DESC_TYPE
            {
                // Avid doesn't support recording descriptors in MXF, so we
                // match what was done in the MXF files and use a tape descriptor
                mapTapeSourceMob(sourcePackage);
            }
        }
    }
    catch (CreateAAFException& ex)
    {
        throw;
    }
    catch (...)
    {
        PA_LOGTHROW(CreateAAFException, ("Failed to add clip to AAF file: unknown exception thrown"));
    }
}

bool AAFFile::addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages, vector<CutInfo> sequence)
{
    bool mcClipWasAdded = false;
    try
    {
        MaterialPackageSet activeMaterialPackages;
        
        // add clips for each material package that includes a track that will be
        // referenced (active) by the multi-camera clip
        MaterialPackageSet::const_iterator iter;
        for (iter = materialPackages.begin(); iter != materialPackages.end(); iter++)
        {
            MaterialPackage* materialPackage = *iter;

            SourcePackage* fileSourcePackage = 0;
            SourcePackage* sourcePackage = 0;
            Track* sourceTrack = 0;

            // Go through tracks of material package.
            vector<Track*>::const_iterator iter2;
            for (iter2 = materialPackage->tracks.begin(); iter2 != materialPackage->tracks.end(); iter2++)
            {
                Track* track = *iter2;
                
                // get the source package and track for this material package track
                if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                {
                    // Go through tracks of the multi-cam group.
                    // First track would generally be the one and only video track.
                    bool donePackage = false;
                    map<uint32_t, MCTrackDef*>::const_iterator iter3;
                    for (iter3 = mcClipDef->trackDefs.begin(); !donePackage && iter3 != mcClipDef->trackDefs.end(); iter3++)
                    {
                        MCTrackDef* trackDef = (*iter3).second;
                        
                        // Now go through the selectors for the track.
                        map<uint32_t, MCSelectorDef*>::const_iterator iter4;
                        for (iter4 = trackDef->selectorDefs.begin(); !donePackage && iter4 != trackDef->selectorDefs.end(); iter4++)
                        {
                            MCSelectorDef* selectorDef = (*iter4).second;
                        
                            if (!selectorDef->isConnectedToSource())
                            {
                                continue;
                            }
                            
                            // if the source package and track match the multi-camera selector
                            if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                                sourceTrack->id == selectorDef->sourceTrackID)
                            {
                                if (activeMaterialPackages.insert(materialPackage).second)
                                {
                                    addClip(materialPackage, packages);
                                }
                                donePackage = true;
                            }
                        }
                    }
                }
            }
        }
        
        
        if (activeMaterialPackages.size() > 0)
        {
            // create composition mob collecting together all mob slots from all master mobs
            // and create a multi-camera clip composition mob
            createMCClip(mcClipDef, activeMaterialPackages, packages, sequence);
            mcClipWasAdded = true;
        }
        
    }
    catch (CreateAAFException& ex)
    {
        throw;
    }
    catch (...)
    {
        PA_LOGTHROW(CreateAAFException, ("Failed to add multi-camera clip to AAF file: unknown exception thrown"));
    }
    
    return mcClipWasAdded;
}
    


void AAFFile::setAppCode(IAAFMob* pMob, aafInt32 value)
{
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFTypeDefInt> pTypeDefInt;
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    
    AAF_CHECK(pMob->QueryInterface(IID_IAAFObject, (void**)&pObject));
    
    // Get AppCode property and type definition
    AAF_CHECK(pCDMob->LookupPropertyDef(kAAFPropID_MobAppCode, &pPropertyDef));
    AAF_CHECK(pPropertyDef->GetTypeDef(&pTypeDef));
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefInt, (void**)&pTypeDefInt));
    
    // Try to get prop value. If it doesn't exist, create it.
    HRESULT hr = pObject->GetPropertyValue(pPropertyDef, &pPropertyValue);
    if(hr != AAFRESULT_SUCCESS)
    {
        if(hr == AAFRESULT_PROP_NOT_PRESENT)
        {
            AAF_CHECK(pTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&value), 
                sizeof(value), &pPropertyValue));
        }
        else
        {
            AAF_CHECK(hr);
        }
    }
    else
    {
        // Property value exists, modify it.
        AAF_CHECK(pTypeDefInt->SetInteger(pPropertyValue, 
            reinterpret_cast<aafMemPtr_t>(&value), sizeof(value)));
    }
    
    // Set property value.
    AAF_CHECK(pObject->SetPropertyValue(pPropertyDef, pPropertyValue));
}

static HRESULT createMobAttrListTaggedValue(IAAFDictionary* const pDict, IAAFMob * const pReferencedMob, IAAFPropertyValue **ppPropertyValue)
{
    // Create tagged value 
    IAAFSmartPointer<IAAFClassDef>    spClassDef;
    IAAFSmartPointer<IAAFTaggedValue>  spTaggedValue;
    IAAFSmartPointer<IAAFTypeDef> spStringDef;
    AAF_CHECK(pDict->LookupClassDef(AUID_AAFTaggedValue, &spClassDef));
    AAF_CHECK(spClassDef->CreateInstance(IID_IAAFTaggedValue, (IUnknown**)&spTaggedValue));
    AAF_CHECK(pDict->LookupTypeDef(kAAFTypeID_String, &spStringDef));
    aafCharacter tag[] = L"_MATCH";
    aafCharacter value[] = L"__PortableObject";
    AAF_CHECK(spTaggedValue->Initialize(tag, spStringDef, (wcslen(value) + 1) * sizeof(aafCharacter), (aafDataBuffer_t)value));

    // Create the strong reference to tagged value
    IAAFSmartPointer<IAAFTypeDef> spType;
    IAAFSmartPointer<IAAFTypeDefObjectRef> spObjectReferenceType;
    AAF_CHECK(pDict->LookupTypeDef(kTaggedValueStrongReferenceTypeID, &spType));
    AAF_CHECK(spType->QueryInterface(IID_IAAFTypeDefObjectRef, (void **)&spObjectReferenceType));
    AAF_CHECK(spObjectReferenceType->CreateValue(spTaggedValue, ppPropertyValue));

    // Set value of PortableObjectClassID property on TaggedValue
    IAAFSmartPointer<IAAFPropertyDef> spPropDef_POCID;
    IAAFSmartPointer<IAAFTypeDef> pTypeDefPOCID;
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFTypeDefInt> pTypeDefInt;
    AAF_CHECK(spClassDef->LookupPropertyDef(kAAFPropID_PortableObjectClassID, &spPropDef_POCID));
    AAF_CHECK(spPropDef_POCID->GetTypeDef(&pTypeDefPOCID));
    AAF_CHECK(pTypeDefPOCID->QueryInterface(IID_IAAFTypeDefInt, (void**)&pTypeDefInt));
    aafUInt32    POCID_value = 0x4d434d52;        // 'M','C','M','R'
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    AAF_CHECK(pTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&POCID_value), sizeof(POCID_value), &pPropertyValue));
    AAF_CHECK(spTaggedValue->QueryInterface(IID_IAAFObject, (void**)&pObject));
    AAF_CHECK(pObject->SetPropertyValue(spPropDef_POCID, pPropertyValue));

    // Create an instance of class Avid_MC_Mob_Reference,
    // and set properties Mob_Reference_Position and Mob_Reference_MobID
    IAAFSmartPointer<IAAFClassDef>      spClassDefAvidMCMR;
    IAAFSmartPointer<IAAFObject>        spObject;
    IAAFSmartPointer<IAAFPropertyDef>   spPropDef;
    IAAFSmartPointer<IAAFTypeDef>       spTypeDef;
    IAAFSmartPointer<IAAFTypeDefInt>    spTypeDefInt;
    AAF_CHECK(pDict->LookupClassDef(kAAFClassID_Avid_MC_Mob_Reference, &spClassDefAvidMCMR));

    // Set Mob_Reference_Position property to value 0
    AAF_CHECK(spClassDefAvidMCMR->LookupPropertyDef(kAAFPropID_Mob_Reference_Position, &spPropDef));
    AAF_CHECK(spPropDef->GetTypeDef(&spTypeDef));
    AAF_CHECK(spTypeDef->QueryInterface(IID_IAAFTypeDefInt, (void **)&spTypeDefInt));
    aafInt64    pos = 0;
    AAF_CHECK(spTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&pos), sizeof(pos), &pPropertyValue));
    AAF_CHECK(spClassDefAvidMCMR->CreateInstance(IID_IAAFObject, (IUnknown**)&spObject));
    AAF_CHECK(spObject->SetPropertyValue(spPropDef, pPropertyValue));

    // Set Mob_Reference_MobID property to the MobID of the passed in Mob
    IAAFSmartPointer<IAAFTypeDefRecord> spTypeDefRecord;
    AAF_CHECK(spClassDefAvidMCMR->LookupPropertyDef(kAAFPropID_Mob_Reference_MobID, &spPropDef));
    AAF_CHECK(spPropDef->GetTypeDef(&spTypeDef));
    AAF_CHECK(spTypeDef->QueryInterface(IID_IAAFTypeDefRecord, (void **)&spTypeDefRecord));
    aafMobIDType_t    uid;
    AAF_CHECK(pReferencedMob->GetMobID(&uid));
    AAF_CHECK(spTypeDefRecord->CreateValueFromStruct((aafMemPtr_t) &uid, sizeof(uid), &pPropertyValue));
    AAF_CHECK(spClassDefAvidMCMR->CreateInstance(IID_IAAFObject, (IUnknown**)&spObject));
    AAF_CHECK(spObject->SetPropertyValue(spPropDef, pPropertyValue));

    // Set value of PortableObject property on TaggedValue
    IAAFSmartPointer<IAAFPropertyDef> spPropDef_PO;
    IAAFSmartPointer<IAAFTypeDef> pTypeDefPO;
    AAF_CHECK(spClassDef->LookupPropertyDef(kAAFPropID_PortableObject, &spPropDef_PO));
    AAF_CHECK(spPropDef_PO->GetTypeDef(&pTypeDefPO));
    AAF_CHECK(spType->QueryInterface(IID_IAAFTypeDefObjectRef, (void **)&spObjectReferenceType));
    AAF_CHECK(spObjectReferenceType->CreateValue(spObject, &pPropertyValue));
    AAF_CHECK(pObject->SetPropertyValue(spPropDef_PO, pPropertyValue));

    return S_OK;
}


void AAFFile::setMobAttributeList(IAAFMob* pMob, IAAFMob* const pRefMob)
{
    // Get the property def for AAFMob::MobAttributeList (from Mob ClassDef)
    IAAFSmartPointer<IAAFClassDef> spClassDefMob;
    IAAFSmartPointer<IAAFPropertyDef> spPropDef_MobAttr;
    AAF_CHECK(pDictionary->LookupClassDef(AUID_AAFMob, &spClassDefMob));
    AAF_CHECK(spClassDefMob->LookupPropertyDef(kAAFPropID_MobAttributeList, &spPropDef_MobAttr));

    //Now, create a property value .......

    //first, get the type def
    //Lookup the VA type
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    AAF_CHECK(pDictionary->LookupTypeDef(kAAFTypeID_TaggedValueStrongReferenceVector, &pTypeDef));

    //Get the VA typedef
    IAAFSmartPointer<IAAFTypeDefVariableArray>  spVA;
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefVariableArray, (void**)&spVA));


    // Setup the array: create an array of strong references to tagged values
    IAAFSmartPointer<IAAFPropertyValue> spElementPropertyValueArray[1];
    IAAFPropertyValue *pElementPropertyValueArray[1]; // copies of pointers "owned by" the smartptr array.
    IAAFSmartPointer<IAAFPropertyValue> spElementPropertyValue;
    IAAFSmartPointer<IAAFPropertyValue> spArrayPropertyValue;

    aafUInt32 i = 0;
    AAF_CHECK(createMobAttrListTaggedValue(pDictionary, pRefMob, &spElementPropertyValue));
    spElementPropertyValueArray[i] = spElementPropertyValue;
    pElementPropertyValueArray[i] = spElementPropertyValueArray[i];

    // Create an empty array and then fill it by appending elements...
    AAF_CHECK(spVA->CreateEmptyValue(&spArrayPropertyValue));
    AAF_CHECK(spVA->AppendElement(spArrayPropertyValue, pElementPropertyValueArray[i]));

    // Set the value VA to the Object
    IAAFSmartPointer<IAAFObject> spObj;
    AAF_CHECK(pMob->QueryInterface(IID_IAAFObject, (void**)&spObj));
    AAF_CHECK(spObj->SetPropertyValue(spPropDef_MobAttr, spArrayPropertyValue));
}

void AAFFile::setDIDInt32Property(IAAFClassDef* pCDDigitalImageDescriptor, 
    IAAFDigitalImageDescriptor2* pDigitalImageDescriptor2, aafUID_t propertyId, aafInt32 value)
{
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFTypeDefInt> pTypeDefInt;
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    
    AAF_CHECK(pDigitalImageDescriptor2->QueryInterface(IID_IAAFObject, (void**)&pObject));
    
    // Get property and type definition
    AAF_CHECK(pCDDigitalImageDescriptor->LookupPropertyDef(propertyId, &pPropertyDef));
    AAF_CHECK(pPropertyDef->GetTypeDef(&pTypeDef));
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefInt, (void**)&pTypeDefInt));
    
    // Try to get prop value. If it doesn't exist, create it.
    HRESULT hr = pObject->GetPropertyValue(pPropertyDef, &pPropertyValue);
    if (hr != AAFRESULT_SUCCESS)
    {
        if (hr == AAFRESULT_PROP_NOT_PRESENT)
        {
            AAF_CHECK(pTypeDefInt->CreateValue(reinterpret_cast<aafMemPtr_t>(&value), 
                sizeof(value), &pPropertyValue));
        }
        else
        {
            // this will throw an exception
            AAF_CHECK(hr);
        }
    }
    else
    {
        // Property value exists, modify it.
        AAF_CHECK(pTypeDefInt->SetInteger(pPropertyValue, 
            reinterpret_cast<aafMemPtr_t>(&value), sizeof(value)));
    }
    
    // Set property value.
    AAF_CHECK(pObject->SetPropertyValue(pPropertyDef, pPropertyValue));
}

void AAFFile::setRGBColor(IAAFCommentMarker* pCommentMarker, int colour)
{
    IAAFSmartPointer<IAAFObject> pObject;
    IAAFSmartPointer<IAAFPropertyDef> pPropertyDef;
    IAAFSmartPointer<IAAFTypeDef> pTypeDef;
    IAAFSmartPointer<IAAFTypeDefRecord> pTypeDefRecord;
    IAAFSmartPointer<IAAFPropertyValue> pPropertyValue;
    RGBColor rgbColor;

    if (colour < 1 || colour > 8)
    {
        rgbColor = g_rgbColors[USER_COMMENT_RED_COLOUR - 1];
    }
    else
    {
        rgbColor = g_rgbColors[colour - 1];
    }
    
    AAF_CHECK(pCommentMarker->QueryInterface(IID_IAAFObject, (void**)&pObject));
    
    // Get property and type definition
    AAF_CHECK(pCDCommentMarker->LookupPropertyDef(kAAFPropID_CommentMarkerColor, &pPropertyDef));
    AAF_CHECK(pPropertyDef->GetTypeDef(&pTypeDef));
    AAF_CHECK(pTypeDef->QueryInterface(IID_IAAFTypeDefRecord, (void**)&pTypeDefRecord));
    
    // Try to get prop value. If it doesn't exist, create it.
    HRESULT hr = pObject->GetPropertyValue(pPropertyDef, &pPropertyValue);
    if (hr != AAFRESULT_SUCCESS)
    {
        if (hr == AAFRESULT_PROP_NOT_PRESENT)
        {
            AAF_CHECK(pTypeDefRecord->CreateValueFromStruct(reinterpret_cast<aafMemPtr_t>(&rgbColor), sizeof(rgbColor), &pPropertyValue));
        }
        else
        {
            // this will throw an exception
            AAF_CHECK(hr);
        }
    }
    else
    {
        // Property value exists, modify it.
        AAF_CHECK(pTypeDefRecord->SetStruct(pPropertyValue, 
            reinterpret_cast<aafMemPtr_t>(&rgbColor), sizeof(rgbColor)));
    }
    
    // Set property value.
    AAF_CHECK(pObject->SetPropertyValue(pPropertyDef, pPropertyValue));
}

bool AAFFile::haveMob(aafMobID_t mobID)
{
    IAAFSmartPointer<IAAFMob> pMob;
    
    HRESULT hr;
    hr = pHeader->LookupMob(mobID, &pMob);
    if (hr == AAFRESULT_SUCCESS)
    {
        return true;
    }
    else if (hr == AAFRESULT_MOB_NOT_FOUND)
    {
        return false;
    }
    
    // this will throw an exception
    AAF_CHECK(hr);
    
    // only the compiler thinks it could get here
    return false;
}

void AAFFile::getReferencedPackages(Package* topPackage, PackageSet& packages, PackageSet& referencedPackages)
{
    vector<Track*>::const_iterator iter;
    for (iter = topPackage->tracks.begin(); iter != topPackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->sourceClip->sourcePackageUID == g_nullUMID)
        {
            // no reference from this track
            continue;
        }
        
        SourcePackage dummy; // dummy acting as key
        dummy.uid = track->sourceClip->sourcePackageUID;
        
        // if reference package in packages then insert and recurse
        PackageSet::iterator result = packages.find(&dummy);
        if (result != packages.end())
        {
            // insert into referenced packages and recurse if not already present
            if (referencedPackages.insert(*result).second)
            {
                getReferencedPackages(*result, packages, referencedPackages);
            }
        }
        
    }
}

bool AAFFile::getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage,
    SourcePackage** sourcePackage, Track** sourceTrack)
{
    if (track->sourceClip->sourcePackageUID == g_nullUMID)
    {
        return false;
    }
    
    SourcePackage dummy; // dummy acting as key
    dummy.uid = track->sourceClip->sourcePackageUID;

    PackageSet::iterator result = packages.find(&dummy);
    if (result != packages.end())
    {
        Package* package = *result;
        Track* trk = package->getTrack(track->sourceClip->sourceTrackID);
        if (trk == 0)
        {
            return false;
        }
        
        // if the package is a tape or live recording then we take that to be the source
        if (package->getType() == SOURCE_PACKAGE)
        {
            SourcePackage* srcPackage = dynamic_cast<SourcePackage*>(package);
            if (srcPackage->descriptor->getType() == TAPE_ESSENCE_DESC_TYPE ||
                srcPackage->descriptor->getType() == LIVE_ESSENCE_DESC_TYPE)
            {
                *sourcePackage = srcPackage;
                *sourceTrack = trk;
                return true;
            }
            else if (srcPackage->descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
            {
                *fileSourcePackage = srcPackage;
            }
        }
        
        // go further down the reference chain to find the tape/live source
        return getSource(trk, packages, fileSourcePackage, sourcePackage, sourceTrack);
    }
        
    return false;
}



// TODO: handle case where master mobs have different start timecodes
void AAFFile::createMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
    vector<CutInfo> sequence)
{
    IAAFSmartPointer<IEnumAAFMobSlots> pEnumMobSlots;
    IAAFSmartPointer<IAAFMob> pCollectionMob;
    IAAFSmartPointer<IAAFMob> pMCMob;
    IAAFSmartPointer<IAAFMob> pSeqMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFMob2> pMob2;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFSmartPointer<IAAFMobSlot> pSequenceMobSlot;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFSmartPointer<IAAFTimelineMobSlot> pCollectionTimelineMobSlot;
    IAAFSmartPointer<IAAFDataDef> pDataDef;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFSequence> pSequence;
    IAAFSmartPointer<IAAFComponent> pComponent;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFSmartPointer<IAAFSelector> pSelector;
    IAAFSmartPointer<IAAFFiller> pFiller;
    IAAFSmartPointer<IAAFTimecode> pTimecode;
    aafRational_t editRate;
    int collectionPictureSlotCount = 0;
    int collectionSoundSlotCount = 0;
    int mcPictureSlotCount = 0;
    int mcSoundSlotCount = 0;
    aafUInt32 trackNumber;
    aafBoolean_t isPicture;
    aafBoolean_t isSound;
    aafSourceRef_t sourceRef;
    aafLength_t length; 
    aafSlotID_t collectionSlotID = 1;
    aafSlotID_t mcSlotID = 1;
    aafSlotID_t mcPictureSlotID = 0;
    bool includeSequence = !sequence.empty();
    aafSlotID_t seqSlotID = 1;
    int seqPictureSlotCount = 0;
    int seqSoundSlotCount = 0;
    int64_t startPosition = 0;
    aafTimecode_t timecode;

    // get the start time which we will use to set the multi-camera name and sequence name
    startPosition = getStartTime(*materialPackages.begin(), packages, _targetEditRate);
    
    // create collection composition mob (Avid application code 5)
    AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pCollectionCompositionMob));
    AAF_CHECK(pCollectionCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pCollectionMob));
    setAppCode(pCollectionMob, 5);
    AAF_CHECK(pHeader->AddMob(pCollectionMob));

    // create multi-camera composition mob (Avid application code 4)
    AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pClipCompositionMob));
    AAF_CHECK(pClipCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pMCMob));
    AAF_CHECK(pClipCompositionMob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
    AAF_CHECK(pMob2->SetUsageCode(kAAFUsage_LowerLevel));
    setAppCode(pMCMob, 4);
    setMobAttributeList(pCollectionMob, pMCMob);
    AAF_CHECK(pMCMob->SetName(createExtendedClipName(mcClipDef->name, startPosition, _targetEditRate)));
    AAF_CHECK(pHeader->AddMob(pMCMob));
    
    if (includeSequence)
    {
        // create multi-camera sequence composition mob
        AAF_CHECK(pCDCompositionMob->CreateInstance(IID_IAAFCompositionMob, (IUnknown **)&pSequenceCompositionMob));
        AAF_CHECK(pSequenceCompositionMob->QueryInterface(IID_IAAFMob, (void **)&pSeqMob));
        AAF_CHECK(pSequenceCompositionMob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
        AAF_CHECK(pMob2->SetUsageCode(kAAFUsage_TopLevel));
        AAF_CHECK(pSeqMob->SetName(createExtendedClipName(mcClipDef->name + "_dc_sequence", startPosition, _targetEditRate)));
        AAF_CHECK(pHeader->AddMob(pSeqMob));
    }
    
    
    // get the maximum track length
    aafLength_t maxPictureLength = 0;
    map<uint32_t, MCTrackDef*>::const_iterator iter1;
    for (iter1 = mcClipDef->trackDefs.begin(); iter1 != mcClipDef->trackDefs.end(); iter1++)
    {
        MCTrackDef* trackDef = (*iter1).second;
        
        map<uint32_t, MCSelectorDef*>::const_iterator iter2;
        for (iter2 = trackDef->selectorDefs.begin(); iter2 != trackDef->selectorDefs.end(); iter2++)
        {
            MCSelectorDef* selectorDef = (*iter2).second;
        
            if (!selectorDef->isConnectedToSource())
            {
                continue;
            }
            
            bool doneSelectedTrack = false;
            MaterialPackageSet::const_iterator iter3;
            for (iter3 = materialPackages.begin(); !doneSelectedTrack && iter3 != materialPackages.end(); iter3++)
            {
                MaterialPackage* materialPackage = *iter3;
            
                vector<Track*>::const_iterator iter4;
                for (iter4 = materialPackage->tracks.begin(); !doneSelectedTrack && iter4 != materialPackage->tracks.end(); iter4++)
                {
                    Track* track = *iter4;
                    
                    SourcePackage* sourcePackage = 0;
                    SourcePackage* fileSourcePackage = 0;
                    Track* sourceTrack = 0;
        
                    // get the source package and track for this material package track
                    if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                    {
                        // if the source package and track match the mult-camera selection
                        if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                            sourceTrack->id == selectorDef->sourceTrackID)
                        {
                            doneSelectedTrack = true;
                            
                            AAF_CHECK(pHeader->LookupMob(convertUMID(materialPackage->uid), &pMob));
                            AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
                            
                            AAF_CHECK(pMobSlot->GetDataDef(&pDataDef));
                            AAF_CHECK(pDataDef->IsPictureKind(&isPicture));
                            AAF_CHECK(pDataDef->IsSoundKind(&isSound));
                            AAF_CHECK(isPicture == kAAFTrue || isSound == kAAFTrue);
                            
                            // add slot to collection composition Mob
                            
                            // get info from MasterMob slot
                            AAF_CHECK(pMobSlot->QueryInterface(IID_IAAFTimelineMobSlot, (void **)&pTimelineMobSlot));
                            AAF_CHECK(pTimelineMobSlot->GetEditRate(&editRate));
                            AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pComponent->GetLength(&length));
                            
                            // update the maximum length
                            if (isPicture == kAAFTrue)
                            {
                                maxPictureLength = (length > maxPictureLength) ? length : maxPictureLength;
                            }
                            else
                            {
                                // round up
                                double factor = _targetEditRate.numerator * editRate.denominator / (double)(_targetEditRate.denominator * editRate.numerator);
                                aafLength_t pictureLength = (aafLength_t)(length * factor);
                                if (length > (aafLength_t)(pictureLength / factor))
                                {
                                    pictureLength++;
                                }

                                maxPictureLength = (pictureLength > maxPictureLength) ? pictureLength : maxPictureLength;
                            }
                        }
                    }
                }
            }
        }
    }
    

    // go through the multi-camera clip definition and add a multi-camera clip slot for tracks present
    // in one of the material packages
    for (iter1 = mcClipDef->trackDefs.begin(); iter1 != mcClipDef->trackDefs.end(); iter1++)
    {
        MCTrackDef* trackDef = (*iter1).second;
        if (DEBUG_PRINT)
        {
            printf("trackDef->index = %d, trackDef->selectorDefs.size() = %zu\n", trackDef->index, trackDef->selectorDefs.size());
        }
        
        map<uint32_t, MCSelectorDef*>::const_iterator iter2;
        for (iter2 = trackDef->selectorDefs.begin(); iter2 != trackDef->selectorDefs.end(); iter2++)
        {
            MCSelectorDef* selectorDef = (*iter2).second;
        
            if (!selectorDef->isConnectedToSource())
            {
                continue;
            }
            
            bool doneSelectedTrack = false;
            MaterialPackageSet::const_iterator iter3;
            for (iter3 = materialPackages.begin(); !doneSelectedTrack && iter3 != materialPackages.end(); iter3++)
            {
                MaterialPackage* materialPackage = *iter3;
            
                vector<Track*>::const_iterator iter4;
                for (iter4 = materialPackage->tracks.begin(); !doneSelectedTrack && iter4 != materialPackage->tracks.end(); iter4++)
                {
                    Track* track = *iter4;
                    
                    SourcePackage* sourcePackage = 0;
                    SourcePackage* fileSourcePackage = 0;
                    Track* sourceTrack = 0;
        
                    // get the source package and track for this material package track
                    if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                    {
                        // if the source package and track match the mult-camera selection
                        if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                            sourceTrack->id == selectorDef->sourceTrackID)
                        {
                            doneSelectedTrack = true;

                            AAF_CHECK(pHeader->LookupMob(convertUMID(materialPackage->uid), &pMob));
                            AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
                            
                            AAF_CHECK(pMobSlot->GetDataDef(&pDataDef));
                            AAF_CHECK(pDataDef->IsPictureKind(&isPicture));
                            AAF_CHECK(pDataDef->IsSoundKind(&isSound));
                            AAF_CHECK(isPicture == kAAFTrue || isSound == kAAFTrue);
                            
                            // add slot to collection composition Mob
                            
                            if (isPicture == kAAFTrue)
                            {
                                collectionPictureSlotCount++;
                                trackNumber = collectionPictureSlotCount;
                            }
                            else
                            {
                                collectionSoundSlotCount++;
                                trackNumber = collectionSoundSlotCount;
                            }
                            
                            // get info from MasterMob slot
                            AAF_CHECK(pMobSlot->QueryInterface(IID_IAAFTimelineMobSlot, (void **)&pTimelineMobSlot));
                            AAF_CHECK(pTimelineMobSlot->GetEditRate(&editRate));
                            AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pComponent->GetLength(&length));
                            if (isSound == kAAFTrue)
                            {
                                // the edit rate for the sound tracks is set to the picture edit rates
                                length = convertLength(convertRational(editRate), length, _targetEditRate);
                            }
                            AAF_CHECK(pMob->GetMobID(&sourceRef.sourceID));
                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                            sourceRef.startTime = 0;
                            
                            // create SourceClip referencing MasterMob slot
                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                            AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                    
                            // create Sequence containing SourceClip
                            AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                            AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                            AAF_CHECK(pSequence->Initialize(pDataDef));
                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                    
                            // append filler if the length < maximum length
                            if (maxPictureLength > length)
                            {
                                // add filler
                                AAF_CHECK(pCDFiller->CreateInstance(IID_IAAFFiller, (IUnknown **)&pFiller));
                                AAF_CHECK(pFiller->Initialize(pDataDef, maxPictureLength - length));
                                AAF_CHECK(pFiller->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                AAF_CHECK(pSequence->AppendComponent(pComponent));
                            }
                            
                            // create TimelineMobSlot with Sequence 
                            AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                            AAF_CHECK(pCollectionMob->AppendNewTimelineSlot(
                                convertRational(_targetEditRate), pSegment, collectionSlotID, 
                                getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                0, &pCollectionTimelineMobSlot));
                            collectionSlotID++;
                            


                            // add slot or selection to multi-camera clip composition Mob
                            
                            length = maxPictureLength;


                            // create SourceClip referencing the collection MobSlot
                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                            sourceRef.startTime = 0;
                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                            AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                            
                            if (AAFRESULT_SUCCEEDED(pMCMob->LookupSlot(trackDef->index, &pMobSlot)))
                            {
                                // get the selector
                                AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                                AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                AAF_CHECK(pSequence->GetComponentAt(0, &pComponent));
                                AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                
                                // append alternate segment
                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                            }
                            else
                            {
                                if (isPicture == kAAFTrue)
                                {
                                    mcPictureSlotCount++;
                                    trackNumber = mcPictureSlotCount;
                                }
                                else
                                {
                                    mcSoundSlotCount++;
                                    trackNumber = mcSoundSlotCount;
                                }
                            
                                if (trackDef->selectorDefs.size() == 1)
                                {
                                    // create Sequence containing SourceClip
                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                    AAF_CHECK(pSequence->Initialize(pDataDef));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                                else
                                {
                                    // create selector
                                    AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                
                                    // set selected segment
                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                    AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                    
                                    // create Sequence containing Selector
                                    AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                    AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                    AAF_CHECK(pComponent->SetLength(length));
                                    AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                    AAF_CHECK(pSequence->Initialize(pDataDef));
                                    AAF_CHECK(pSequence->AppendComponent(pComponent));
                                }
                                
                                // create TimelineMobSlot with Sequence 
                                // Note: the slot name follows the generated trackNumber, which could be
                                // different from the trackDef->number
                                AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                AAF_CHECK(pMCMob->AppendNewTimelineSlot(
                                    convertRational(_targetEditRate), pSegment, mcSlotID, 
                                    getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                    0, &pTimelineMobSlot));
                                if (trackDef->number != 0)
                                {
                                    AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                    AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                }
                                
                                if (mcPictureSlotID < 1 && isPicture)
                                {
                                    mcPictureSlotID = mcSlotID;
                                }
                                
                                mcSlotID++;

                            }
                            
                            
                            // add slot or selection to multi-camera sequence composition Mob
                            
                            if (includeSequence)
                            {
                                if (DEBUG_PRINT)
                                {
                                    printf("sequence size = %zu\n", sequence.size());
                                    printf("isPicture     = %s\n", (isPicture ? "true" : "false"));
                                }
                                if (sequence.size() > 1 && (isPicture || _addAudioEdits))
                                {
                                    if (trackDef->selectorDefs.size() > 1 &&
                                        AAFRESULT_SUCCEEDED(pSeqMob->LookupSlot(trackDef->index, &pSequenceMobSlot)))
                                    {
                                        // sequence is present, and therefore we add a selector
                                        if (DEBUG_PRINT)
                                        {
                                            printf("sequence present\n");
                                        }
                                        
                                        size_t i;
                                        aafLength_t sourceClipLength;
                                        aafLength_t totalSourceClipLength = 0;
                                        CutInfo current;
                                        CutInfo next; 
                                        for (i = 0; i < sequence.size(); i++)
                                        {
                                            if (totalSourceClipLength >= length)
                                            {
                                                // end of sequence
                                                break;
                                            }
                                            
                                            current = sequence[i];
                                            if (i != sequence.size() - 1)
                                            {
                                                // calculate length of segment using next cut in sequence
                                                
                                                next = sequence[i + 1];
                                                
                                                // calculate length of segment
                                                if (next.position > length)
                                                {
                                                    // next cut is beyond the end of the sequence so the clip
                                                    // extends to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the next cut
                                                    sourceClipLength = next.position - current.position;
                                                }
                                            }
                                            else
                                            {
                                                // calculate length of segment using length of total clip
                                                
                                                if (current.position > length)
                                                {
                                                    // cut is beyond end of sequence
                                                    // NOTE: we shouldn't be here because the first cut should be
                                                    // at position 0 and the previous clip will have extended
                                                    // right to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the start of the next cut
                                                    sourceClipLength = length - current.position;
                                                }
                                            }
                                            
                                            totalSourceClipLength += sourceClipLength;

                                            
                                            // create SourceClip referencing the collection MobSlot
                                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                            sourceRef.startTime = current.position;
                                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                            AAF_CHECK(pSourceClip->Initialize(pDataDef, sourceClipLength, sourceRef));
                                            
                                            // get the selector
                                            AAF_CHECK(pSequenceMobSlot->GetSegment(&pSegment));
                                            AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                            AAF_CHECK(pSequence->GetComponentAt(i, &pComponent));
                                            AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                            
                                            if (isPicture && // if sound then the first track became the SelectedSegment
                                                selectorDef->sourceConfig->name.compare(sequence[i].source) == 0)
                                            {
                                                // set selected segment
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                                AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                                            }
                                            else
                                            {
                                                // append alternate segment
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                                AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                            }
                                        }
                                    }
                                    else
                                    {
                                        // sequence not present or only 1 selector is present
                                        if (DEBUG_PRINT)
                                        {
                                            printf("sequence not present or only one selector; creating sequence\n");
                                        }
                                        
                                        if (isPicture == kAAFTrue)
                                        {
                                            seqPictureSlotCount++;
                                            trackNumber = seqPictureSlotCount;
                                        }
                                        else
                                        {
                                            seqSoundSlotCount++;
                                            trackNumber = seqSoundSlotCount;
                                        }

                                        // create Sequence
                                        AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                        AAF_CHECK(pSequence->Initialize(pDataDef));
                                        
                                        // create TimelineMobSlot with Sequence 
                                        // Note: the slot name follows the generated trackNumber, which could be
                                        // different from the trackDef->number
                                        AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                        AAF_CHECK(pSeqMob->AppendNewTimelineSlot(
                                            convertRational(_targetEditRate), pSegment, seqSlotID, 
                                            getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                            0, &pTimelineMobSlot));
                                        if (trackDef->number != 0)
                                        {
                                            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                        }
                                        seqSlotID++;

                                        // add Selectors or SourceClip to Sequence
                                        size_t i;
                                        aafLength_t sourceClipLength;
                                        aafLength_t totalSourceClipLength = 0;
                                        CutInfo current;
                                        CutInfo next; 
                                        for (i = 0; i < sequence.size(); i++)
                                        {
                                            if (totalSourceClipLength >= length)
                                            {
                                                // end of sequence
                                                break;
                                            }
                                            
                                            if (DEBUG_PRINT)
                                            {
                                                printf("sequence element %zu\n", i);
                                            }
                                            current = sequence[i];
                                            if (i != sequence.size() - 1)
                                            {
                                                // calculate length of segment using next cut in sequence
                                                
                                                next = sequence[i + 1];
                                                if (DEBUG_PRINT)
                                                {
                                                    printf("Not last cut in sequence\n");
                                                    printf("next.position = %"PRIi64"\n", next.position);
                                                }
                                                
                                                // calculate length of segment
                                                if (next.position > length)
                                                {
                                                    // next cut is beyond the end of the sequence so the clip
                                                    // extends to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the next cut
                                                    sourceClipLength = next.position - current.position;
                                                }
                                            }
                                            else
                                            {
                                                // calculate length of segment using length of total clip
                                                if (DEBUG_PRINT)
                                                {
                                                    printf("Last cut in sequence\n");
                                                }
                                                
                                                if (current.position > length)
                                                {
                                                    // cut is beyond end of sequence
                                                    // NOTE: we shouldn't be here because the first cut should be
                                                    // at position 0 and the previous clip will have extended
                                                    // right to the end
                                                    sourceClipLength = length - totalSourceClipLength;
                                                }
                                                else
                                                {
                                                    // clip ends at the start of the next cut
                                                    sourceClipLength = length - current.position;
                                                }
                                            }
                                            if (DEBUG_PRINT)
                                            {
                                                printf("sourceClipLength %"PRId64"\n",sourceClipLength);
                                            }
                                            totalSourceClipLength += sourceClipLength;
                                            

                                            // create SourceClip referencing the collection MobSlot
                                            AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                            AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                            sourceRef.startTime = current.position;
                                            AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                            AAF_CHECK(pSourceClip->Initialize(pDataDef, sourceClipLength, sourceRef));
                                            
                                            if (trackDef->selectorDefs.size() > 1)
                                            {
                                                // create selector
                                                AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                                AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                                AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                                AAF_CHECK(pComponent->SetLength(sourceClipLength));
                                                
                                                if (!isPicture || // the first sound track always becomes the SelectedSegment
                                                    selectorDef->sourceConfig->name.compare(sequence[i].source) == 0)
                                                {
                                                    // set selected segment
                                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                                    AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                                                }
                                                else
                                                {
                                                    // append alternate segment
                                                    AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                                    AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                                }
                                            }
                                            else
                                            {
                                                // source clip is appended to sequence
                                                AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                            }
                            
                                            AAF_CHECK(pSequence->AppendComponent(pComponent));
                                        } //for (i = 0; i < sequence.size(); i++)
                                    }
                                }
                                
                                // single cut, which should be at position 0, so sequence contains one clip
                                else
                                {
                                    // create SourceClip referencing the collection MobSlot
                                    AAF_CHECK(pCollectionTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                    AAF_CHECK(pCollectionMob->GetMobID(&sourceRef.sourceID));
                                    AAF_CHECK(pMobSlot->GetSlotID(&sourceRef.sourceSlotID));
                                    sourceRef.startTime = 0;
                                    AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
                                    AAF_CHECK(pSourceClip->Initialize(pDataDef, length, sourceRef));
                                    
                                    if (AAFRESULT_SUCCEEDED(pSeqMob->LookupSlot(trackDef->index, &pMobSlot)))
                                    {
                                        // get the selector
                                        AAF_CHECK(pMobSlot->GetSegment(&pSegment));
                                        AAF_CHECK(pSegment->QueryInterface(IID_IAAFSequence, (void **)&pSequence));
                                        AAF_CHECK(pSequence->GetComponentAt(0, &pComponent));
                                        AAF_CHECK(pComponent->QueryInterface(IID_IAAFSelector, (void **)&pSelector));
                                        
                                        // append alternate segment
                                        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFComponent, (void **)&pSegment));
                                        AAF_CHECK(pSelector->AppendAlternateSegment(pSegment));
                                    }
                                    else
                                    {
                                        if (isPicture == kAAFTrue)
                                        {
                                            seqPictureSlotCount++;
                                            trackNumber = seqPictureSlotCount;
                                        }
                                        else
                                        {
                                            seqSoundSlotCount++;
                                            trackNumber = seqSoundSlotCount;
                                        }

                                        // always create a sequence containing a selector even when the trackDef->selectorDefs.size() 
                                        // equals 1 because the Avid was found to show the mxf filename in the audio track instead of the clip
                                        // name if a sequence with a source clip was used instead
                                        
                                        // create selector
                                        AAF_CHECK(pCDSelector->CreateInstance(IID_IAAFSelector, (IUnknown **)&pSelector));
                                    
                                        // set selected segment
                                        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                        AAF_CHECK(pSelector->SetSelectedSegment(pSegment));
                        
                                        // create Sequence containing Selector
                                        AAF_CHECK(pSelector->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
                                        AAF_CHECK(pComponent->SetDataDef(pDataDef));
                                        AAF_CHECK(pComponent->SetLength(length));
                                        AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
                                        AAF_CHECK(pSequence->Initialize(pDataDef));
                                        AAF_CHECK(pSequence->AppendComponent(pComponent));
                                        
                                        // create TimelineMobSlot with Sequence 
                                        // Note: the slot name follows the generated trackNumber, which could be
                                        // different from the trackDef->number
                                        AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
                                        AAF_CHECK(pSeqMob->AppendNewTimelineSlot(
                                            convertRational(_targetEditRate), pSegment, seqSlotID, 
                                            getSlotName(isPicture == kAAFTrue, false, trackNumber),
                                            0, &pTimelineMobSlot));
                                        if (trackDef->number != 0)
                                        {
                                            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
                                            AAF_CHECK(pMobSlot->SetPhysicalNum(trackDef->number));
                                        }
                                        seqSlotID++;
                                    }
                                }
                            } //if (includeSequence)
                        }
                    }
                }
            }
            // TODO: log warning if no selected track found?
        }
    }

    // set start timecode for sequence by appending a timecode slot in sequence composition mob
    if (includeSequence)
    {
        timecode.startFrame = startPosition;
        timecode.drop = kAAFTcNonDrop; // TODO: don't hardcode
        timecode.fps = getTimecodeBase(_targetEditRate);
        AAF_CHECK(pCDTimecode->CreateInstance(IID_IAAFTimecode, (IUnknown **)&pTimecode));
        AAF_CHECK(pTimecode->Initialize(maxPictureLength, &timecode));
        AAF_CHECK(pTimecode->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        
        AAF_CHECK(pSequenceCompositionMob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
        AAF_CHECK(pMob2->AppendNewTimelineSlot(convertRational(_targetEditRate), pSegment, seqSlotID, L"TC1", 0, &pTimelineMobSlot));
        AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
        AAF_CHECK(pMobSlot->SetPhysicalNum(1));
        seqSlotID++;        
    }

    // add user comments to the multi-camera composition mob
    vector<UserComment> userComments = mergeUserComments(materialPackages);
    mapUserComments(pMCMob, userComments, mcPictureSlotID >= 1 ? mcPictureSlotID : mcSlotID);
    
    // add the same user comments to the sequence composition mob
    if (includeSequence)
    {
        mapUserComments(pSeqMob, userComments, mcPictureSlotID >= 1 ? mcPictureSlotID : mcSlotID);
    }
}

void AAFFile::mergeLocatorUserComment(vector<UserComment>& userComments, const UserComment& newComment)
{
    if (newComment.position < 0)
    {
        // not a locator user comment
        return;
    }
    
    vector<UserComment>::iterator commentIter;        
    for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
    {
        UserComment& userComment = *commentIter;
        
        if (newComment.position < userComment.position)
        {
            userComments.insert(commentIter, newComment);
            return;
        }
        if (newComment.position == userComment.position)
        {
            // first comment at a position is taken, all others are ignored
            return;
        }
    }
    
    // position of new comment is beyond the current list of comments
    userComments.push_back(newComment);        
}

vector<UserComment> AAFFile::mergeUserComments(MaterialPackageSet& materialPackages)
{
    vector<UserComment> mergedComments;
    
    // merge locator comments
    MaterialPackageSet::const_iterator packageIter;
    for (packageIter = materialPackages.begin(); packageIter != materialPackages.end(); packageIter++)
    {
        MaterialPackage* materialPackage = *packageIter;
        
        vector<UserComment> userComments = materialPackage->getUserComments();
        
        vector<UserComment>::const_iterator commentIter;
        for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
        {
            const UserComment& userComment = *commentIter;
            
            if (userComment.position >= 0)
            {
                mergeLocatorUserComment(mergedComments, userComment);
            }
        }
    }

    // copy static comments from the first package
    packageIter = materialPackages.begin(); 
    if (packageIter != materialPackages.end())
    {
        MaterialPackage* materialPackage = *packageIter;
        
        vector<UserComment> userComments = materialPackage->getUserComments();
        
        vector<UserComment>::const_iterator commentIter;
        for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
        {
            const UserComment& userComment = *commentIter;
            
            if (userComment.position < 0)
            {
                mergedComments.push_back(userComment);
            }
        }
    }
    
    return mergedComments;
}

aafUInt32 AAFFile::getUserCommentsDescribedSlotId(Package* package)
{
    // get the video slot described by the comments 
    // note: we assume there is a video track and otherwise the first audio is selected as the
    // target for the comment markers

    aafUInt32 describedSlotId = 0;
    vector<Track*>::const_iterator trackIter;
    for (trackIter = package->tracks.begin(); trackIter != package->tracks.end(); trackIter++)
    {
        Track* track = *trackIter;
        
        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            describedSlotId = track->id;
            break;
        }
        else if (describedSlotId == 0 && track->dataDef == SOUND_DATA_DEFINITION)
        {
            describedSlotId = track->id;
        }
    }
    
    return describedSlotId;
}

void AAFFile::mapUserComments(IAAFMob* mob, vector<UserComment> userComments, aafUInt32 describedSlotId)
{
    IAAFSmartPointer<IAAFMob2> pMob2;
    IAAFSmartPointer<IAAFEventMobSlot> pEventMobSlot;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFSmartPointer<IAAFSequence> pSequence;
    IAAFSmartPointer<IAAFDescriptiveMarker> pDescriptiveMarker;
    IAAFSmartPointer<IAAFCommentMarker> pCommentMarker;
    IAAFSmartPointer<IAAFEvent> pEvent;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFSmartPointer<IAAFComponent> pComponent;
    
    // map static user comments (and check for positioned comments)
    bool havePositionedComments = false;
    vector<UserComment>::const_iterator commentIter;
    for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
    {
        const UserComment& userComment = *commentIter;
        
        if (userComment.position >= 0)
        {
            havePositionedComments = true;
            continue;
        }
        
        AAF_CHECK(mob->AppendComment(convertString(userComment.name), convertString(userComment.value))); 
    }
    
    // map positioned comments
    if (havePositionedComments)
    {
        AAF_CHECK(pCDSequence->CreateInstance(IID_IAAFSequence, (IUnknown **)&pSequence));
        AAF_CHECK(pSequence->Initialize(pDescriptiveMetadataDef));
        
        for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
        {
            const UserComment& userComment = *commentIter;
            
            if (userComment.position < 0)
            {
                continue;
            }

            // add comment marker to sequence
            
            AAF_CHECK(pCDDescriptiveMarker->CreateInstance(IID_IAAFDescriptiveMarker, (IUnknown **)&pDescriptiveMarker));
            AAF_CHECK(pDescriptiveMarker->QueryInterface(IID_IAAFCommentMarker, (void **)&pCommentMarker));
            AAF_CHECK(pDescriptiveMarker->QueryInterface(IID_IAAFEvent, (void **)&pEvent));
            AAF_CHECK(pDescriptiveMarker->QueryInterface(IID_IAAFComponent, (void **)&pComponent));
            
            AAF_CHECK(pComponent->SetDataDef(pDescriptiveMetadataDef));
            
            AAF_CHECK(pEvent->SetPosition(userComment.position));
            AAF_CHECK(pEvent->SetComment(convertString(userComment.value)));

            aafUInt32 slots[1];
            slots[0] = describedSlotId;
            AAF_CHECK(pDescriptiveMarker->SetDescribedSlotIDs(1, slots));
            
            setRGBColor(pCommentMarker, userComment.colour);

            
            AAF_CHECK(pSequence->AppendComponent(pComponent));
        }

        // add sequence of comments to a event slot 
        AAF_CHECK(pSequence->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        AAF_CHECK(mob->QueryInterface(IID_IAAFMob2, (void **)&pMob2));
        AAF_CHECK(pMob2->AppendNewEventSlot(convertRational(_targetEditRate), pSegment, 1000, L"", 0, &pEventMobSlot));
        AAF_CHECK(pEventMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
        AAF_CHECK(pMobSlot->SetPhysicalNum(1));
    }
    
}

void AAFFile::mapMasterMob(MaterialPackage* materialPackage)
{
    IAAFSmartPointer<IAAFMasterMob> pMasterMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFDataDef* pDataDef = NULL;

    // return if the material package has already been mapped to a mob
    if (haveMob(convertUMID(materialPackage->uid)))
    {
        return;
    }
    
    
    // create master mob    
    AAF_CHECK(pCDMasterMob->CreateInstance(IID_IAAFMasterMob, (IUnknown **)&pMasterMob));
    AAF_CHECK(pMasterMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(materialPackage->uid)));
    AAF_CHECK(pMob->SetName(convertString(materialPackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(materialPackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    aafUInt32 describedSlotId = getUserCommentsDescribedSlotId(materialPackage);
    mapUserComments(pMob, materialPackage->getUserComments(), describedSlotId);
    
    // create mob slots
    vector<Track*>::const_iterator iter;
    for (iter = materialPackage->tracks.begin(); iter != materialPackage->tracks.end(); iter++)
    {
        Track* track = *iter;

        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        
        // create a source clip referencing the file source mob
        aafSourceRef_t sourceRef;
        sourceRef.sourceID = convertUMID(track->sourceClip->sourcePackageUID);
        sourceRef.sourceSlotID = track->sourceClip->sourceTrackID;
        sourceRef.startTime = track->sourceClip->position;
        AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
        AAF_CHECK(pSourceClip->Initialize(pDataDef, track->sourceClip->length, sourceRef));
        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        
        // create mob slot        
        AAF_CHECK(pMob->AppendNewTimelineSlot(convertRational(track->editRate), pSegment, track->id,
            convertString(track->name), 0, &pTimelineMobSlot));
        if (track->number != 0)
        {
            AAF_CHECK(pTimelineMobSlot->QueryInterface(IID_IAAFMobSlot, (void **)&pMobSlot));
            AAF_CHECK(pMobSlot->SetPhysicalNum(track->number));
        }
    }        
}

    
void AAFFile::mapFileSourceMob(SourcePackage* sourcePackage)
{
    IAAFSmartPointer<IAAFSourceMob> pFileSourceMob;
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFSourceClip> pSourceClip;
    IAAFSmartPointer<IAAFSegment> pSegment;
    IAAFSmartPointer<IAAFCDCIDescriptor> pCDCIDescriptor;
    IAAFSmartPointer<IAAFCDCIDescriptor2> pCDCIDescriptor2;
    IAAFSmartPointer<IAAFDigitalImageDescriptor2> pDigitalImageDescriptor2;
    IAAFSmartPointer<IAAFPCMDescriptor> pPCMDescriptor;
    IAAFSmartPointer<IAAFSoundDescriptor> pSoundDescriptor;
    IAAFSmartPointer<IAAFFileDescriptor> pFileDescriptor;
    IAAFSmartPointer<IAAFEssenceDescriptor> pEssenceDescriptor;
    IAAFSmartPointer<IAAFTimelineMobSlot> pTimelineMobSlot;
    IAAFDataDef* pDataDef = NULL;
    aafRational_t audioSamplingRate = {48000, 1};
    // TODO: see below - image size for video with variable size frames
    
    // return if the file source package has already been mapped to a mob
    if (haveMob(convertUMID(sourcePackage->uid)))
    {
        return;
    }
    
    FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
    if (fileDescriptor == 0)
    {
        PA_LOGTHROW(CreateAAFException, ("File source package has a null file essence descriptor"));
    }
    
    // create file source mob    
    AAF_CHECK(pCDSourceMob->CreateInstance(IID_IAAFSourceMob, (IUnknown **)&pFileSourceMob));
    AAF_CHECK(pFileSourceMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(sourcePackage->uid)));
    AAF_CHECK(pMob->SetName(convertString(sourcePackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(sourcePackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    aafUInt32 describedSlotId = getUserCommentsDescribedSlotId(sourcePackage);
    mapUserComments(pMob, sourcePackage->getUserComments(), describedSlotId);
    
    // create mob slots
    vector<Track*>::const_iterator iter;
    for (iter = sourcePackage->tracks.begin(); iter != sourcePackage->tracks.end(); iter++)
    {
        Track* track = *iter;

        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        
        // create a source clip referencing the tape source mob
        aafSourceRef_t sourceRef;
        sourceRef.sourceID = convertUMID(track->sourceClip->sourcePackageUID);
        sourceRef.sourceSlotID = track->sourceClip->sourceTrackID;
        sourceRef.startTime = track->sourceClip->position;
        AAF_CHECK(pCDSourceClip->CreateInstance(IID_IAAFSourceClip, (IUnknown **)&pSourceClip));
        AAF_CHECK(pSourceClip->Initialize(pDataDef, track->sourceClip->length, sourceRef));
        AAF_CHECK(pSourceClip->QueryInterface(IID_IAAFSegment, (void **)&pSegment));
        
        // create mob slot. Note: (physical) track number is ignored for file source mobs        
        AAF_CHECK(pMob->AppendNewTimelineSlot(convertRational(track->editRate), pSegment, track->id,
            convertString(track->name), 0, &pTimelineMobSlot));
    
        // create the file descriptor
        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            // TODO: match level of support in writeavidmxf
            // only PAL supported for now
            assert(track->editRate == g_palEditRate);

            if (fileDescriptor->imageAspectRatio.numerator == 0 ||
                fileDescriptor->imageAspectRatio.denominator == 0)
            {
                PA_LOGTHROW(CreateAAFException, ("File descriptor image aspect ratio %d%d is invalid",
                    fileDescriptor->imageAspectRatio.numerator, fileDescriptor->imageAspectRatio.denominator));
            }
            
            AAF_CHECK(pCDCDCIDescriptor->CreateInstance(IID_IAAFCDCIDescriptor2, (IUnknown **)&pCDCIDescriptor));
            AAF_CHECK(pCDCIDescriptor->QueryInterface(IID_IAAFCDCIDescriptor2, (void **)&pCDCIDescriptor2));
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFDigitalImageDescriptor2, (void **)&pDigitalImageDescriptor2));
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFFileDescriptor, (void **)&pFileDescriptor));
    
            AAF_CHECK(pCDCIDescriptor2->Initialize());
            if (fileDescriptor->videoResolutionID == DV25_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(2));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_IEC_DV_625_50));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(288, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {23, 335};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 144000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 144000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x8d);
            }
            else if (fileDescriptor->videoResolutionID == DV50_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DV_Based_50Mbps_625_50));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(288, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {23, 335};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 288000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 288000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x8e);
            }
            else if (fileDescriptor->videoResolutionID == DVCPROHD_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DV_Based_100Mbps_1080x50I_INGEX));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(540, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(540, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {21, 584};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 576000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 576000);
            }
            else if (fileDescriptor->videoResolutionID == IMX30_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_IMX_30));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(304, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 16));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {7, 320};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 150000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 150000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0xa2);
            }
            else if (fileDescriptor->videoResolutionID == IMX40_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_IMX_40));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(304, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 16));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {7, 320};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 200000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 200000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0xa1);
            }
            else if (fileDescriptor->videoResolutionID == IMX50_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_IMX_50));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(304, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 16));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {7, 320};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 250000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 250000);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0xa0);
            }
            else if (fileDescriptor->videoResolutionID == UNC_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(592, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(576, 720, 0, 16));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFMixedFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetFieldStartOffset(7680));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 860160); // aligned frame size
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 860160);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0xaa);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG21_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_21));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4c);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG31_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_31));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4d);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG101_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_101));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4b);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG101M_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_101m));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 288));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 288, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFOneField));
                aafInt32 videoLineMap[1] = {15};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(1, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x6e);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG151S_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_151s));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 352));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 352, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFOneField));
                aafInt32 videoLineMap[1] = {15};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(1, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x4e);
            }
            else if (fileDescriptor->videoResolutionID == MJPEG201_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_Avid_MJPEG_201));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(296, 720));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(288, 720, 0, 8));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {15, 328};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(1));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 0); // variable frame size
                // TODO: don't have this size
                // setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    // kAAFPropID_DIDImageSize, track->length * 144000); 
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 0x52);
            }
            else if (fileDescriptor->videoResolutionID == DNX36p_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(1080, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(1080, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFFullFrame));
                aafInt32 videoLineMap[2] = {42, 0};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DNxHD));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 188416);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 188416);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 1253);
            }
            else if (fileDescriptor->videoResolutionID == DNX120p_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(1080, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(1080, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFFullFrame));
                aafInt32 videoLineMap[2] = {42, 0};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DNxHD));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 606208);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 606208);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 1237);
            }
            else if (fileDescriptor->videoResolutionID == DNX185p_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(1080, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(1080, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFFullFrame));
                aafInt32 videoLineMap[2] = {42, 0};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DNxHD));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 917504);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 917504);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 1238);
            }
            else if (fileDescriptor->videoResolutionID == DNX120i_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(540, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(540, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {21, 584};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DNxHD));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 606208);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 606208);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 1242);
            }
            else if (fileDescriptor->videoResolutionID == DNX185i_MATERIAL_RESOLUTION)
            {
                AAF_CHECK(pCDCIDescriptor2->SetHorizontalSubsampling(2));
                AAF_CHECK(pCDCIDescriptor2->SetVerticalSubsampling(1));
                AAF_CHECK(pDigitalImageDescriptor2->SetStoredView(540, 1920));
                AAF_CHECK(pDigitalImageDescriptor2->SetDisplayView(540, 1920, 0, 0));
                AAF_CHECK(pDigitalImageDescriptor2->SetFrameLayout(kAAFSeparateFields));
                aafInt32 videoLineMap[2] = {21, 584};
                AAF_CHECK(pDigitalImageDescriptor2->SetVideoLineMap(2, videoLineMap));
                AAF_CHECK(pDigitalImageDescriptor2->SetImageAlignmentFactor(8192));
                AAF_CHECK(pDigitalImageDescriptor2->SetCompression(kAAFCompressionDef_DNxHD));
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDFrameSampleSize, 917504);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDImageSize, track->sourceClip->length * 917504);
                setDIDInt32Property(pCDDigitalImageDescriptor, pDigitalImageDescriptor2,
                    kAAFPropID_DIDResolutionID, 1243);
            }
            else
            {
                PA_LOGTHROW(CreateAAFException, ("Unsupported resolution id %d", fileDescriptor->videoResolutionID));
            }
            AAF_CHECK(pCDCIDescriptor2->SetComponentWidth(8));
            AAF_CHECK(pCDCIDescriptor2->SetColorSiting(kAAFRec601));
            AAF_CHECK(pCDCIDescriptor2->SetBlackReferenceLevel(16));
            AAF_CHECK(pCDCIDescriptor2->SetWhiteReferenceLevel(235));
            AAF_CHECK(pCDCIDescriptor2->SetColorRange(225));
            AAF_CHECK(pDigitalImageDescriptor2->SetImageAspectRatio(convertRational(fileDescriptor->imageAspectRatio)));
            
            AAF_CHECK(pFileDescriptor->SetLength(track->sourceClip->length));
            AAF_CHECK(pFileDescriptor->SetSampleRate(convertRational(track->editRate)));
            AAF_CHECK(pFileDescriptor->SetContainerFormat(pAAFKLVContainerDef));
            
            AAF_CHECK(pCDCIDescriptor2->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssenceDescriptor));
        }
        else // SOUND_DATA_DEFINITION
        {
            AAF_CHECK(pCDPCMDescriptor->CreateInstance(IID_IAAFPCMDescriptor, (IUnknown **)&pPCMDescriptor));
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFSoundDescriptor, (void **)&pSoundDescriptor));
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFFileDescriptor, (void **)&pFileDescriptor));
            
            AAF_CHECK(pPCMDescriptor->Initialize());
            AAF_CHECK(pPCMDescriptor->SetBlockAlign((fileDescriptor->audioQuantizationBits + 7) / 8));
            AAF_CHECK(pPCMDescriptor->SetAverageBPS(audioSamplingRate.numerator * ((fileDescriptor->audioQuantizationBits + 7) / 8) / 
                audioSamplingRate.denominator));
            AAF_CHECK(pSoundDescriptor->SetChannelCount(1));
            AAF_CHECK(pSoundDescriptor->SetAudioSamplingRate(audioSamplingRate));
            // TODO: first set this value in MXF - do we know if it is locked or not? 
            // AAF_CHECK(pSoundDescriptor->SetIsLocked(kAAFTrue));
            AAF_CHECK(pSoundDescriptor->SetQuantizationBits(fileDescriptor->audioQuantizationBits));
            
            AAF_CHECK(pFileDescriptor->SetLength(track->sourceClip->length));
            AAF_CHECK(pFileDescriptor->SetSampleRate(convertRational(track->editRate)));
            AAF_CHECK(pFileDescriptor->SetContainerFormat(pAAFKLVContainerDef));
            
            AAF_CHECK(pPCMDescriptor->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssenceDescriptor));
        }

        AAF_CHECK(pFileSourceMob->SetEssenceDescriptor(pEssenceDescriptor));
    }        
}

void AAFFile::mapTapeSourceMob(SourcePackage* sourcePackage)
{
    IAAFSmartPointer<IAAFMob> pMob;
    IAAFSmartPointer<IAAFTapeDescriptor> pTapeDesc;
    IAAFSmartPointer<IAAFEssenceDescriptor> pEssDesc;
    IAAFSmartPointer<IAAFMobSlot> pMobSlot;
    IAAFDataDef* pDataDef = NULL;
    aafUInt32 maxSlotID = 1;
    aafRational_t pictureEditRate = convertRational(_targetEditRate); // if no picture then edit rate is ssumed to be _targetEditRate 
    // TODO: we also not drop frame flag indicator if it is NTSC
    
    // return if the tape source package has already been mapped to a mob
    if (haveMob(convertUMID(sourcePackage->uid)))
    {
        return;
    }
    
    // create source mob
    AAF_CHECK(pCDSourceMob->CreateInstance(IID_IAAFSourceMob, (IUnknown **)&pTapeSourceMob));
    AAF_CHECK(pTapeSourceMob->QueryInterface(IID_IAAFMob, (void **)&pMob));
    AAF_CHECK(pMob->SetMobID(convertUMID(sourcePackage->uid)));
    AAF_CHECK(pMob->SetName(convertString(sourcePackage->name)));
    AAF_CHECK(pMob->SetCreateTime(convertTimestamp(sourcePackage->creationDate)));
    AAF_CHECK(pHeader->AddMob(pMob));

    // user comments
    aafUInt32 describedSlotId = getUserCommentsDescribedSlotId(sourcePackage);
    mapUserComments(pMob, sourcePackage->getUserComments(), describedSlotId);
    
    // append tape descriptor        
    AAF_CHECK(pCDTapeDescriptor->CreateInstance(IID_IAAFTapeDescriptor, (IUnknown **)&pTapeDesc));
    AAF_CHECK(pTapeDesc->QueryInterface(IID_IAAFEssenceDescriptor, (void **)&pEssDesc));
    AAF_CHECK(pTapeSourceMob->SetEssenceDescriptor(pEssDesc));

    // append picture and sound slots
    vector<Track*>::const_iterator iter;
    for (iter = sourcePackage->tracks.begin(); iter != sourcePackage->tracks.end(); iter++)
    {
        Track* track = *iter;
        
        if (track->dataDef == PICTURE_DATA_DEFINITION)
        {
            pDataDef = pPictureDef;
            pictureEditRate = convertRational(track->editRate);
        }
        else // SOUND_DATA_DEFINITION
        {
            pDataDef = pSoundDef;
        }
        

        // create mob slot        
        AAF_CHECK(pTapeSourceMob->AddNilReference(track->id, track->sourceClip->length, 
            pDataDef, convertRational(track->editRate)));
        AAF_CHECK(pMob->LookupSlot(track->id, &pMobSlot));
        AAF_CHECK(pMobSlot->SetName(convertString(track->name)));
        if (track->number != 0)
        {
            AAF_CHECK(pMobSlot->SetPhysicalNum(track->number));
        }
        
        if (track->id > maxSlotID)
        {
            maxSlotID = track->id;
        }
    }
    
    // append timecode slot
    // Note: we assume the same was done in the MXF file
    aafTimecode_t startTimecode;
    startTimecode.startFrame = 0;
    startTimecode.drop = 0; // TODO: need drop frame indicator
    startTimecode.fps = getTimecodeBase(convertRational(pictureEditRate));
    AAF_CHECK(pTapeSourceMob->AppendTimecodeSlot(pictureEditRate, maxSlotID + 1, startTimecode, 
        120 * 60 * 60 * startTimecode.fps));
    AAF_CHECK(pMob->LookupSlot(maxSlotID + 1, &pMobSlot));
    AAF_CHECK(pMobSlot->SetName(L"TC1"));
    
}



