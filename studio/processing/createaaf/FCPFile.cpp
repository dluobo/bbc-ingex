/*
 * $Id: FCPFile.cpp,v 1.6 2009/09/18 17:05:47 philipn Exp $
 *
 * Final Cut Pro XML file for defining clips, multi-camera clips, etc
 *
 * Copyright (C) 2008, BBC, Nicholas Pinks <npinks@users.sourceforge.net>
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

#define __STDC_FORMAT_MACROS 1

#include <cassert>
#include <sstream>
#include <vector>

#include "FCPFile.h"
#include "CutsDatabase.h"
#include "CreateAAFException.h"
#include "package_utils.h"

#include <Logging.h>
#include <XmlTools.h>
#include <Timecode.h>
#include <Database.h>
#include <MCClipDef.h>
#include <Utilities.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#define TESTING_FLAG 1

using namespace std;
using namespace prodauto;
using namespace xercesc;


//} settingsStruct;

FCPFile::FCPFile(string filename, string fcpPath)
: _filename(filename), _impl(0), _doc(0), _rootElem(0), _fcpPath(fcpPath), _idName(1)
{
    XmlTools::Initialise();
    
    _impl =  DOMImplementationRegistry::getDOMImplementation(Xml("Core"));
    _doc = _impl->createDocument(
            0,                  // root element namespace URI.
            Xml("xmeml"),         // root element name
            0);                 // document type object (DTD).

    _rootElem = _doc->getDocumentElement();
    _rootElem->setAttribute(Xml("version"), Xml("2"));
}

FCPFile::~FCPFile()
{
    _doc->release();
    
    XmlTools::Terminate();
}

void FCPFile::save()
{
    XmlTools::DomToFile(_doc, _filename.c_str());
}


void FCPFile::mergeLocatorUserComment(vector<UserComment>& userComments, const UserComment& newComment)
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
		    //first comment at a position is taken, all others are ignored
		    return;
	    }
    }
    
    //position of new comment is beyond the current list of comments
    userComments.push_back(newComment);
}

vector<UserComment> FCPFile::mergeUserComments(MaterialPackageSet& materialPackages)
{
    vector<UserComment> mergedComments;
    
    //merge locator comments
    MaterialPackageSet::const_iterator packageIter;
    for (packageIter = materialPackages.begin(); packageIter != materialPackages.end(); packageIter++)
    {
	    MaterialPackage* materialPackage = *packageIter;
	    
	    vector<UserComment> userComments = materialPackage->getUserComments();
	    
	    vector<UserComment>::const_iterator commentIter;
	    for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
	    {
		    const UserComment& userComment = *commentIter;
		    
		    if(userComment.position >= 0)
		    {
			    mergeLocatorUserComment(mergedComments, userComment);
		    }
	    }
    }
    
    return mergedComments;
}

//int FCPFile::getSettings(MaterialPackage* topPackage, void * settings, PackageSet& packages)
//int FCPFile::getSettings(MaterialPackage* topPackage, settingsStruct * settings, PackageSet& packages)
int FCPFile::getSettings(MaterialPackage* topPackage, PackageSet& packages)
{
    
    _ntsc = "FALSE";
    _audioTracks = 0;
    _videoTracks = 0;
    _trackIndex =0;
    _df = "NDF";
    _field = "lower";
    
    vector<prodauto::Track*>::const_iterator iter;
    for (iter = topPackage->tracks.begin(); iter != topPackage->tracks.end(); iter++)
    {
	prodauto::Track * track = *iter;
	prodauto::Rational fR = track->editRate;
	
	SourcePackage dummy;
	dummy.uid = track->sourceClip->sourcePackageUID;
    	PackageSet::iterator result = packages.find(&dummy);
    	Package* package = *result;
    
    	SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);	
        FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
    
	if (track->dataDef == PICTURE_DATA_DEFINITION)
	{
	    _frameRate = fR.numerator / fR.denominator;
	    if (fR.numerator % fR.denominator > 0)
	    {
	        _df = "DF";
	        _ntsc = "TRUE";
	        _frameRate = _frameRate + 1;
		_field = "upper";
	    }
	
	    if (fileDescriptor->imageAspectRatio == prodauto::g_16x9ImageAspect) {
	        _aspect = "TRUE";
	    } else {
		_aspect = "FALSE";
	    }
	    
	    ostringstream dur_ss;
            dur_ss << track->sourceClip->length;
	    _dur = dur_ss.str().c_str();
	    
	    if (fileDescriptor->videoResolutionID == 2) {  //DV25
		_width = "720";
		_height = "576";
		_pixelAspect = "PAL-601";
		_fieldDominance = "lower";
	//		_codecName = "Apple DV - PAL"
		_appName = "Final Cut Pro";
		_appManufacturer = "Apple";
		_appVersion = "5.0";
		_codecName = "Apple DV";
		_codecType = "DV-PAL";
		_codecSpatial ="0";
		_codecTemporal ="0";
		_codecKeyframe ="0";
		_codecDatarate ="0";
		_anamorphic ="TRUE";
	    }
	    else if (fileDescriptor->videoResolutionID == 5) { //DV50
		_width = "720";
		_height = "576";
		_pixelAspect = "PAL-601";
		_fieldDominance = "lower";
	//		_codecName = "Apple DV - PAL"
		_appName = "Final Cut Pro";
		_appManufacturer = "Apple";
		_appVersion = "5.0";
		_codecName = "Apple DV";
		_codecType = "DV-PAL";
		_codecSpatial ="0";
		_codecTemporal ="0";
		_codecKeyframe ="0";
		_codecDatarate ="0";
		_anamorphic ="TRUE";
	    }
	    else if (fileDescriptor->videoResolutionID == 19) { //DVCPRO100
		_width = "1440";
		_height = "1080";
		_pixelAspect = "HD-(1440x1080)";
		_fieldDominance = "lower";
	//		_codecName = "Apple DV - PAL"
		_appName = "Final Cut Pro";
		_appManufacturer = "Apple";
		_appVersion = "5.0";
		_codecName = "DV - PAL";
		_codecType = "dvcp";
		_codecSpatial ="1023";
		_codecTemporal ="0";
		_codecKeyframe ="0";
		_codecDatarate ="0";
		_anamorphic ="FALSE";
	    }
	    _videoTracks++;
            string floc = fileDescriptor->fileLocation.c_str();
            size_t pos = floc.find_last_of("/");
	    if(pos != string::npos)
            {
                _filelocation = floc.substr(pos + 1);
            }

            else
            {
                _filelocation = floc;
            }
	}
	else if (track->dataDef == SOUND_DATA_DEFINITION)
        {
            _audioTracks ++;
	    _sample ="48000";
	    _depth ="16";
        }
	else
        {
            continue;
        }
	_trackIndex ++;
    }


return 1;
//return settings;
}

void FCPFile::addClip(MaterialPackage* materialPackage, PackageSet& packages)
{

    MaterialPackage *topPackage = materialPackage;
    getSettings(topPackage, packages);
//#if TESTING_FLAG
   
    static const char * src = "source";
    
    int64_t st_time;
    int angle = 0;
    int counter = 1;
    
    angle++;
    ostringstream angle_ss;
    angle_ss << angle; 
    
    ostringstream frameRate_ss;
    frameRate_ss << _frameRate;
    
    //
    // GENERATE TAGS FOR XML
    //
    DOMElement * clipElem = _doc->createElement(Xml("clip"));
    

    DOMElement * durElem = _doc->createElement(Xml("duration")); //Duration
    DOMElement * mediaElem = _doc->createElement(Xml("media")); //Media Tag
    DOMElement * videoElem = _doc->createElement(Xml("video")); //Video Tag
    DOMElement * audioElem = _doc->createElement(Xml("audio")); //Audio Tag
    DOMElement * nameElem = _doc->createElement(Xml("name"));  //name
    
    DOMElement * inElem = _doc->createElement(Xml("in"));  //in
    DOMElement * outElem = _doc->createElement(Xml("out")); //out
    DOMElement * startElem = _doc->createElement(Xml("start")); //start
    DOMElement * endElem = _doc->createElement(Xml("end")); //end
    DOMElement * pixelElem = _doc->createElement(Xml("pixelaspectratio")); //pixel aspect
    DOMElement * enableElem = _doc->createElement(Xml("enabled")); //enabled 
    DOMElement * anamorphicElem = _doc->createElement(Xml("anamorphic")); //anamorphic
    DOMElement * alphaElem = _doc->createElement(Xml("alphatype")); //alpha type
    DOMElement * masterElem = _doc->createElement(Xml("masterclipid")); //masterclip

    
    //
    // SET RATE PROPERTIES, NOT CHANGED THROUGH OUT XML
    //
    DOMElement * rateElem = _doc->createElement(Xml("rate"));  //Rate Tag
    DOMElement * ntscElem = _doc->createElement(Xml("ntsc")); //rate
    DOMElement * tbElem = _doc->createElement(Xml("timebase"));
    ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
    tbElem->appendChild(_doc->createTextNode(Xml(frameRate_ss.str().c_str())));
    rateElem->appendChild(ntscElem);
    rateElem->appendChild(tbElem);
    
    //
    // SET TIMECODE PROPERTIES, NOT CHANGED THROUGH OUT XML
    //
    DOMElement * timecodeElem = _doc->createElement(Xml("timecode"));
    DOMElement * tcElem = _doc->createElement(Xml("string"));
    DOMElement * dfElem = _doc->createElement(Xml("displayformat"));
    DOMElement * scElem = _doc->createElement(Xml("source"));
    DOMElement * frElem = _doc->createElement(Xml("frame"));
    st_time = getStartTime(topPackage, packages, g_palEditRate); //start frames from midnite
    Timecode tc(st_time);  //returns true time code
    
    tcElem->appendChild(_doc->createTextNode(Xml(tc.Text())));
    dfElem->appendChild(_doc->createTextNode(Xml(_df.c_str())));
    scElem->appendChild(_doc->createTextNode(Xml(src)));
    ostringstream st_time_ss;
    st_time_ss << st_time; 
    frElem->appendChild(_doc->createTextNode(Xml(st_time_ss.str().c_str())));
    timecodeElem->appendChild(rateElem);
    timecodeElem->appendChild(tcElem);
    timecodeElem->appendChild(dfElem);
    timecodeElem->appendChild(scElem);
    timecodeElem->appendChild(frElem);
    

    //
    // SET SAMPLE (Video and Audio) PROPERTIES, NOT CHANGED THROUGH OUT XML
    //
    DOMElement * VsampleElem = _doc->createElement(Xml("samplecharacteristics"));
    DOMElement * widthElem = _doc->createElement(Xml("width"));
    DOMElement * heightElem = _doc->createElement(Xml("height"));
    widthElem->appendChild(_doc->createTextNode(Xml(_width.c_str())));
    heightElem->appendChild(_doc->createTextNode(Xml(_height.c_str())));
    VsampleElem->appendChild(widthElem);
    VsampleElem->appendChild(heightElem);

    //
    //Open XML Clip
    //
    _rootElem->appendChild(clipElem);
    
    //
    //PART1
    //START OF XML WITH <clip id> 
    //
    //Tags set: <clip id>
    //		<name>
    //		<duration>
    //		<rate>
    //			<ntsc>
    //			<timebase>
    //		</rate>	
    //		<in> = STATIC 0
    //		<out> = DURATION
    //		<start> = STATIC 0
    //		<end> = DURATION
    //		<pixelaspectratio>
    //		<enabled> = STATIC TRUE
    //		<anamorphic>
    //		<alphatype> = STATIC NONE
    //		<matserclipid> = NAME
    
    clipElem->setAttribute(Xml("id"), Xml(_filelocation.c_str()));
    nameElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str()))); //name of toppackage = clip name
    durElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); //duration set in getsettings()
    inElem->appendChild(_doc->createTextNode(Xml("0"))); // in
    outElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // out
    startElem->appendChild(_doc->createTextNode(Xml("0"))); // start
    endElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // end   
    pixelElem->appendChild(_doc->createTextNode(Xml(_pixelAspect.c_str()))); // pixelaspect set in getsettings()
    enableElem->appendChild(_doc->createTextNode(Xml("TRUE"))); // enable = STATOC TRUE
    anamorphicElem->appendChild(_doc->createTextNode(Xml(_anamorphic.c_str()))); // anamorohpic
    alphaElem->appendChild(_doc->createTextNode(Xml("NONE"))); // alphatype = STATIC NONE
    masterElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str())); // matsterClip id
    
    //
    // Set place above tags into:   <xmeml>
    //				       <clip id>	
    //					    <tags!>
    //
    
    
    clipElem->appendChild(nameElem);
    clipElem->appendChild(durElem);
    clipElem->appendChild(rateElem);
    clipElem->appendChild(inElem);
    clipElem->appendChild(outElem);
    clipElem->appendChild(startElem);
    clipElem->appendChild(endElem);
    clipElem->appendChild(pixelElem);
    clipElem->appendChild(enableElem);
    clipElem->appendChild(anamorphicElem);
    clipElem->appendChild(alphaElem);
    clipElem->appendChild(masterElem);
    
    //
    // NOW SET TRACK INFO, EITHER VIDEO OR AUDIO
    //
    // NOTE: WILL NOT WORK WITH MORE THAN ONE VIDEO TRACK 
    //
    
    
    //
    //PART2
    //VIDEO 
    //
    if (_videoTracks >= 1)
    {
	//video, track and clipitem Tags
	DOMElement * _trackElem = _doc->createElement(Xml("track"));
	DOMElement * _clipElem = _doc->createElement(Xml("clipitem"));
	DOMElement * _mediaElem = _doc->createElement(Xml("media"));
	
	DOMElement * _durElem = _doc->createElement(Xml("duration")); //Duration
	DOMElement * _nameElem = _doc->createElement(Xml("name"));  //name
	DOMElement * _inElem = _doc->createElement(Xml("in"));  //in
	DOMElement * _outElem = _doc->createElement(Xml("out")); //out
	DOMElement * _startElem = _doc->createElement(Xml("start")); //start
	DOMElement * _endElem = _doc->createElement(Xml("end")); //end
	DOMElement * _pixelElem = _doc->createElement(Xml("pixelaspectratio")); //pixel aspect
	DOMElement * _enableElem = _doc->createElement(Xml("enabled")); //enabled 
	DOMElement * _anamorphicElem = _doc->createElement(Xml("anamorphic")); //anamorphic
	DOMElement * _alphaElem = _doc->createElement(Xml("alphatype")); //alpha type
	DOMElement * _masterElem = _doc->createElement(Xml("masterclipid")); //masterclip
	
	//
	// SET RATE PROPERTIES, NOT CHANGED THROUGH OUT XML
	//
	DOMElement * _rateElem = _doc->createElement(Xml("rate"));  //Rate Tag
	DOMElement * _ntscElem = _doc->createElement(Xml("ntsc")); //rate
	DOMElement * _tbElem = _doc->createElement(Xml("timebase"));
	_ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
	_tbElem->appendChild(_doc->createTextNode(Xml(frameRate_ss.str().c_str())));
	_rateElem->appendChild(_ntscElem);
	_rateElem->appendChild(_tbElem);
	
	_clipElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str()));
	_nameElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str()))); //name of toppackage = clip name
	_durElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); //duration set in getsettings()
	_inElem->appendChild(_doc->createTextNode(Xml("0"))); // in
	_outElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // out
	_startElem->appendChild(_doc->createTextNode(Xml("0"))); // start
	_endElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // end   
	_pixelElem->appendChild(_doc->createTextNode(Xml(_pixelAspect.c_str()))); // pixelaspect set in getsettings()
	_enableElem->appendChild(_doc->createTextNode(Xml("TRUE"))); // enable = STATOC TRUE
	_anamorphicElem->appendChild(_doc->createTextNode(Xml(_anamorphic.c_str()))); // anamorohpic
	_alphaElem->appendChild(_doc->createTextNode(Xml("NONE"))); // alphatype = STATIC NONE
	_masterElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str())); // matsterClip id
		
	//
	// Use clip settings for required XML values
	// Clipitem id = name + count
	ostringstream count_ss;
	count_ss << counter; 
	std::string Clipitemid = _filelocation.c_str() + count_ss.str();
	_clipElem->setAttribute(Xml("id"), Xml(Clipitemid.c_str()));
	
	//Make required xml structure
	_clipElem->appendChild(_nameElem);
	_clipElem->appendChild(_durElem);
	_clipElem->appendChild(_rateElem);
	_clipElem->appendChild(_inElem);
	_clipElem->appendChild(_outElem);
	_clipElem->appendChild(_startElem);
	_clipElem->appendChild(_endElem);
	_clipElem->appendChild(_pixelElem);
	_clipElem->appendChild(_enableElem);
	_clipElem->appendChild(_anamorphicElem);
	_clipElem->appendChild(_alphaElem);
	_clipElem->appendChild(_masterElem);
	
	//file items
	//PART 2.1
	//Set file values, id and purl
	DOMElement * _fileElem = _doc->createElement(Xml("file"));
	DOMElement * _purlElem = _doc->createElement(Xml("pathurl")); //File Path
	
	
	_fileElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str()));
	_purlElem->appendChild(_doc->createTextNode(Xml(_fcpPath.c_str())));
	_purlElem->appendChild(_doc->createTextNode(Xml(_filelocation.c_str())));
	
	//Make required xml structure for file tags
	DOMElement * _f_durElem = _doc->createElement(Xml("duration")); //Duration
	DOMElement * _f_nameElem = _doc->createElement(Xml("name"));  //name
	DOMElement * _f_rateElem = _doc->createElement(Xml("rate"));  //Rate Tag
	DOMElement * _f_ntscElem = _doc->createElement(Xml("ntsc")); //rate
	DOMElement * _f_tbElem = _doc->createElement(Xml("timebase"));
	
	_f_ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
	_f_tbElem->appendChild(_doc->createTextNode(Xml(frameRate_ss.str().c_str())));
	_f_rateElem->appendChild(_f_ntscElem);
	_f_rateElem->appendChild(_f_tbElem);
	_f_nameElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str()))); //name of toppackage = clip name
	_f_durElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); //duration set in getsettings()
	
	_fileElem->appendChild(_f_nameElem);
	_fileElem->appendChild(_purlElem);
	_fileElem->appendChild(_f_rateElem);
	_fileElem->appendChild(_f_durElem);
	_fileElem->appendChild(timecodeElem);
	
	//media items
	//PART2.2
	//For each video and audio track set media info
	
	
	for (int _vCount = _videoTracks; _vCount > 0; _vCount--)
	{
	    //
	    //PART2.2-1 Set video info
	    //<duration>
	    //<samplecharacteristics>
	    //  <width>
	    //  <height>
	    //
	    DOMElement * __videoElem = _doc->createElement(Xml("video"));
	    DOMElement * _V_durElem = _doc->createElement(Xml("duration"));
	    _V_durElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); //duration set in getsettings()
	    __videoElem->appendChild(_V_durElem);
	    __videoElem->appendChild(VsampleElem);	
	    _mediaElem->appendChild(__videoElem); //video into <media>
	    _fileElem->appendChild(_mediaElem); //media into file	
		
	}
	for (int _aCount = _audioTracks; _aCount > 0; _aCount--)
	{
	    //
	    //PART2.2-2 Set audio info
	    //<duration>
	    //<samplecharacteristics>
	    //  <samplerate>
	    //  <depth>
	    //<channelcount>
	    //
	    DOMElement * __audioElem = _doc->createElement(Xml("audio"));
	    DOMElement * __chCountElem = _doc->createElement(Xml("channelcount"));
	    DOMElement * _AsampleElem = _doc->createElement(Xml("samplecharacteristics"));
	    DOMElement * _smplRateElem = _doc->createElement(Xml("samplerate"));
	    DOMElement * _depthElem = _doc->createElement(Xml("depth"));
	    _smplRateElem->appendChild(_doc->createTextNode(Xml(_sample.c_str())));
	    _depthElem->appendChild(_doc->createTextNode(Xml(_depth.c_str())));
	    _AsampleElem->appendChild(_smplRateElem);
	    _AsampleElem->appendChild(_depthElem);
	    __chCountElem->appendChild(_doc->createTextNode(Xml("1")));
	    __audioElem->appendChild(audioElem);
	    __audioElem->appendChild(_AsampleElem);
	    __audioElem->appendChild(__chCountElem);
	    _mediaElem->appendChild(__audioElem); //audio into <media>
	    _fileElem->appendChild(_mediaElem); //media into file
	}
	
	_clipElem->appendChild(_fileElem); //file into clip
	
	//
	//Source track = DESCRIBES VIDEO AS PRIMARY
	//
	DOMElement * _sourceElem = _doc->createElement(Xml("sourcetrack"));
	DOMElement * _mediaTypeElem = _doc->createElement(Xml("mediatype"));
	_mediaTypeElem->appendChild(_doc->createTextNode(Xml("video")));
	_sourceElem->appendChild(_mediaTypeElem); 
	_clipElem->appendChild(_sourceElem);
	
	//LINK
	//PART 2.3
	//
	int i = 1;
	int iii = 1;
	for (int _vCount = _videoTracks; _vCount > 0; _vCount--)
	{
	    //
	    //PART2.3-1 Set video LINK
	    //<linkclipref>
	    //<mediatype>
	    //<trackindex>
	    //<cliptype>
	    //
	    
	    
	    
	    DOMElement * __linkElem = _doc->createElement(Xml("link"));
	    DOMElement * __linkcliprefElem = _doc->createElement(Xml("linkclipref"));
	    DOMElement * __mediatypeElem = _doc->createElement(Xml("mediatype"));
	    DOMElement * __trackindexElem = _doc->createElement(Xml("trackindex"));
	    DOMElement * __clipindexElem = _doc->createElement(Xml("clipindex"));
	    
	    ostringstream i_ss;
	    i_ss << i; 
	    
	    ostringstream iii_ss;
	    iii_ss << iii; 
	    std::string linkName = _filelocation.c_str() + iii_ss.str();
	    __linkcliprefElem->appendChild(_doc->createTextNode(Xml(linkName.c_str())));
	    __mediatypeElem->appendChild(_doc->createTextNode(Xml("video")));
	    __trackindexElem->appendChild(_doc->createTextNode(Xml(i_ss.str().c_str())));
	    __clipindexElem->appendChild(_doc->createTextNode(Xml("1")));
	    
	    __linkElem->appendChild(__linkcliprefElem);
	    __linkElem->appendChild(__mediatypeElem);
	    __linkElem->appendChild(__trackindexElem);
	    __linkElem->appendChild(__clipindexElem);
	    
	    _clipElem->appendChild(__linkElem); //link into <clip>
	    
	    i++;
	    iii++;
	}
	i = 1;
	for (int _aCount = _audioTracks; _aCount > 0; _aCount--)
	{
	    //
	    //PART2.3-1 Set audio LINK
	    //<linkclipref>
	    //<mediatype>
	    //<trackindex>
	    //<cliptype>
	    //
	    
	    
	    
	    
	    DOMElement * __linkElem = _doc->createElement(Xml("link"));
	    DOMElement * __linkcliprefElem = _doc->createElement(Xml("linkclipref"));
	    DOMElement * __mediatypeElem = _doc->createElement(Xml("mediatype"));
	    DOMElement * __trackindexElem = _doc->createElement(Xml("trackindex"));
	    DOMElement * __clipindexElem = _doc->createElement(Xml("clipindex"));
	    
	    ostringstream iii_ss;
	    iii_ss << iii; 
	    
	    ostringstream i_ss;
	    i_ss << i; 
	    std::string linkName = _filelocation.c_str() + iii_ss.str();
	    __linkcliprefElem->appendChild(_doc->createTextNode(Xml(linkName.c_str())));
	    __mediatypeElem->appendChild(_doc->createTextNode(Xml("audio")));
	    __trackindexElem->appendChild(_doc->createTextNode(Xml(i_ss.str().c_str())));
	    __clipindexElem->appendChild(_doc->createTextNode(Xml("1")));
	    
	    __linkElem->appendChild(__linkcliprefElem);
	    __linkElem->appendChild(__mediatypeElem);
	    __linkElem->appendChild(__trackindexElem);
	    __linkElem->appendChild(__clipindexElem);
	    
	    _clipElem->appendChild(__linkElem); //link into <clip>
	    
	    i++;
	    iii++;
	}
	
	//
	//PART 2.4
	//<field dominance>
	//<enabled>
	//<locked>
	//
	DOMElement * __enableElem = _doc->createElement(Xml("enabled"));
	DOMElement * _lockedElem = _doc->createElement(Xml("locked"));
	DOMElement * _fieldElem = _doc->createElement(Xml("fielddominance"));
	    
	__enableElem->appendChild(_doc->createTextNode(Xml("TRUE")));
	_lockedElem->appendChild(_doc->createTextNode(Xml("FALSE")));
	_fieldElem->appendChild(_doc->createTextNode(Xml(_field.c_str())));
	    
	_trackElem->appendChild(__enableElem); //enabled into <track>
	_trackElem->appendChild(_lockedElem); //locked into <track>
	_clipElem->appendChild(_fieldElem); //locked into <clip>
	    
	_trackElem->appendChild(_clipElem); //<clip> into <track>
	videoElem->appendChild(_trackElem); //<track> into <video>
	mediaElem->appendChild(videoElem); //<video> into <media>
	//
	// AUDIO TO FOLLOW...
	//
	
    }
    
    // PART3
    // AUDIO
    //
    // <audio>
    //     <in>
    //     <out>
    //     <track>
    //         ... REPEAT OF VIDEO FOR EACH TRACK ...
    //	   </track>
    // </audio>
    
    
    if (_audioTracks >= 1)
    {
	    
	    
	//
	//Set Audio Format
	//
	DOMElement * _inElem = _doc->createElement(Xml("in"));
	DOMElement * _outElem = _doc->createElement(Xml("out"));
	_inElem->appendChild(_doc->createTextNode(Xml("-1")));
	_outElem->appendChild(_doc->createTextNode(Xml("-1")));
	
	audioElem->appendChild(_inElem);
	audioElem->appendChild(_outElem);
	
	//
	//Audio Tracks
	//One copy of infomation per track
	//Works with Multiple Audio Tracks
	//
	int ii = 0;
	for (int _aCount = _audioTracks; _aCount > 0; _aCount--)
	{
	    if (_videoTracks == 0)
	    {
		counter = 0;
	    }
	    counter++;
	    ii++;
	    //
	    // Set XML to:
	    // <track>	
	    //     <clipitem id = >
	    //		<name>
	    //		<duration>
	    //		<rate>
	    //		    <ntsc>
	    //		    <timebase>
	    //		</rate>
	    //		<in>
	    //		<out>
	    //		<start>
	    //		<end>
	    //		<enabled>
	    //		<masterclip id>
	    //		<file id>
	    //		<sourcetrack>
	    //		    <mediatype info>
	    //		    <trackindex>  (note incrament)
	    //		</sourcetrack>
	    //		<link> SEE BELLOW!!
	    //
	    
	    //track and clipitem Tags
	    DOMElement * _trackElem = _doc->createElement(Xml("track"));
	    DOMElement * _clipElem = _doc->createElement(Xml("clipitem"));
	    DOMElement * _fileElem = _doc->createElement(Xml("file"));
	    
	    //
	    // Use clip settings for required XML values
	    // Clipitem id = name + count
	    ostringstream count_ss;
	    count_ss << counter; 
	    std::string Clipitemid = _filelocation.c_str() + count_ss.str();
	    _clipElem->setAttribute(Xml("id"), Xml(Clipitemid.c_str()));
	    
	    
	    DOMElement * __nameElem = _doc->createElement(Xml("name"));  //name
	    DOMElement * __inElem = _doc->createElement(Xml("in"));  //in
	    DOMElement * __outElem = _doc->createElement(Xml("out")); //out
	    DOMElement * __startElem = _doc->createElement(Xml("start")); //start
	    DOMElement * __endElem = _doc->createElement(Xml("end")); //end
	    DOMElement * ___enableElem = _doc->createElement(Xml("enabled")); //enabled
	    DOMElement * __masterElem = _doc->createElement(Xml("masterclipid")); //masterclip
	    DOMElement * __durElem = _doc->createElement(Xml("duration")); //Duration
	    
	    __nameElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str()))); //name of toppackage = clip name
	    __durElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); //duration set in getsettings()
	    __inElem->appendChild(_doc->createTextNode(Xml("0"))); // in
	    __outElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // out
	    __startElem->appendChild(_doc->createTextNode(Xml("0"))); // start
	    __endElem->appendChild(_doc->createTextNode(Xml(_dur.c_str()))); // end   
	    ___enableElem->appendChild(_doc->createTextNode(Xml("TRUE"))); // enable = STATOC TRUE
	    __masterElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str())); // matsterClip id
	    
	    //
	    // SET RATE PROPERTIES, NOT CHANGED THROUGH OUT XML
	    //
	    DOMElement * __rateElem = _doc->createElement(Xml("rate"));  //Rate Tag
	    DOMElement * __ntscElem = _doc->createElement(Xml("ntsc")); //rate
	    DOMElement * __tbElem = _doc->createElement(Xml("timebase"));
	    __ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
	    __tbElem->appendChild(_doc->createTextNode(Xml(frameRate_ss.str().c_str())));
	    __rateElem->appendChild(__ntscElem);
	    __rateElem->appendChild(__tbElem);
	    
	    
	    //Make required xml structure
	    _clipElem->appendChild(__nameElem);
	    _clipElem->appendChild(__durElem);
	    _clipElem->appendChild(__rateElem);
	    _clipElem->appendChild(__inElem);
	    _clipElem->appendChild(__outElem);
	    _clipElem->appendChild(__startElem);
	    _clipElem->appendChild(__endElem);
	    _clipElem->appendChild(___enableElem);
	    _clipElem->appendChild(__masterElem);
	    _fileElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str()));
	    _clipElem->appendChild(_fileElem);
	    //
	    //Source track = DESCRIBES AUDIO AS PRIMARY
	    //
	    DOMElement * _sourceElem = _doc->createElement(Xml("sourcetrack"));
	    DOMElement * _mediaTypeElem = _doc->createElement(Xml("mediatype"));
	    DOMElement * _trackIndexElem = _doc->createElement(Xml("trackindex"));
	    _mediaTypeElem->appendChild(_doc->createTextNode(Xml("audio")));
	    ostringstream ii_ss;
	    ii_ss << ii; 
	    _trackIndexElem->appendChild(_doc->createTextNode(Xml(ii_ss.str().c_str())));
	    _sourceElem->appendChild(_mediaTypeElem);
	    _trackIndexElem->appendChild(_trackIndexElem); 
	    _clipElem->appendChild(_sourceElem);
	    
	    //
	    // LINK
	    // SAME AS VIDEO
	    //
	    int i = 1;
	    int iii = 1;
	    for (int _vCount = _videoTracks; _vCount > 0; _vCount--)
	    {
		//
		//Set audio LINK
		//<linkclipref>
		//<mediatype>
		//<trackindex>
		//<cliptype>
		//
	    
		
	    
		DOMElement * __linkElem = _doc->createElement(Xml("link"));
		DOMElement * __linkcliprefElem = _doc->createElement(Xml("linkclipref"));
		DOMElement * __mediatypeElem = _doc->createElement(Xml("mediatype"));
		DOMElement * __trackindexElem = _doc->createElement(Xml("trackindex"));
		DOMElement * __clipindexElem = _doc->createElement(Xml("clipindex"));
		
		ostringstream iii_ss;
		iii_ss << iii; 
		
		ostringstream i_ss;
		i_ss << i; 
		std::string linkName = _filelocation.c_str() + iii_ss.str();
		__linkcliprefElem->appendChild(_doc->createTextNode(Xml(linkName.c_str())));
		__mediatypeElem->appendChild(_doc->createTextNode(Xml("video")));
		__trackindexElem->appendChild(_doc->createTextNode(Xml(i_ss.str().c_str())));
		__clipindexElem->appendChild(_doc->createTextNode(Xml("1")));
		    
		__linkElem->appendChild(__linkcliprefElem);
		__linkElem->appendChild(__mediatypeElem);
		__linkElem->appendChild(__trackindexElem);
		__linkElem->appendChild(__clipindexElem);
		    
		_clipElem->appendChild(__linkElem); //link into <clip>
		    
		i++;
		iii++;
	    }
	    i = 1;
	    for (int _aCount = _audioTracks; _aCount > 0; _aCount--)
	    {
		//
		//Set audio LINK
		//<linkclipref>
		//<mediatype>
		//<trackindex>
		//<cliptype>
		//
		    
		
		
		DOMElement * __linkElem = _doc->createElement(Xml("link"));
		DOMElement * __linkcliprefElem = _doc->createElement(Xml("linkclipref"));
		DOMElement * __mediatypeElem = _doc->createElement(Xml("mediatype"));
		DOMElement * __trackindexElem = _doc->createElement(Xml("trackindex"));
		DOMElement * __clipindexElem = _doc->createElement(Xml("clipindex"));
		
		ostringstream iii_ss;
		iii_ss << iii; 
		
		ostringstream i_ss;
		i_ss << i; 
		std::string linkName = _filelocation.c_str() + iii_ss.str();
		__linkcliprefElem->appendChild(_doc->createTextNode(Xml(linkName.c_str())));
		__mediatypeElem->appendChild(_doc->createTextNode(Xml("audio")));
		__trackindexElem->appendChild(_doc->createTextNode(Xml(i_ss.str().c_str())));
		__clipindexElem->appendChild(_doc->createTextNode(Xml("1")));
		    
		__linkElem->appendChild(__linkcliprefElem);
		__linkElem->appendChild(__mediatypeElem);
		__linkElem->appendChild(__trackindexElem);
		__linkElem->appendChild(__clipindexElem);
		    
		_clipElem->appendChild(__linkElem); //link into <clip>
		    
		i++;
		iii++;
	    }
	    //
	    //<enabled>
	    //<locked>
	    //
	    DOMElement * __enableElem = _doc->createElement(Xml("enabled"));
	    DOMElement * _lockedElem = _doc->createElement(Xml("locked"));
	    DOMElement * _fieldElem = _doc->createElement(Xml("fielddominance"));
	    
	    __enableElem->appendChild(_doc->createTextNode(Xml("TRUE")));
	    _lockedElem->appendChild(_doc->createTextNode(Xml("FALSE")));
	    _fieldElem->appendChild(_doc->createTextNode(Xml(_field.c_str())));
	    
	    _trackElem->appendChild(__enableElem); //enabled into <track>
	    _trackElem->appendChild(_lockedElem); //locked into <track>
	
	    _trackElem->appendChild(_clipElem); //<clip> into track>
	    audioElem->appendChild(_trackElem); //<track> into <audio>
	    
	}
	mediaElem->appendChild(audioElem); //<audio> into <media>
    }
    clipElem->appendChild(mediaElem); //media into <clip>

/*	
    //khgfchgcgchgcmhgcmh
    
    DOMElement * fnameElem = _doc->createElement(Xml("name"));
    clipElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str()));
    
    DOMElement * anaElem = _doc->createElement(Xml("anamorphic"));
    anaElem->appendChild(_doc->createTextNode(Xml(_aspect.c_str())));
   

        DOMElement *durElem1 = _doc->createElement(Xml("duration"));
    
    
    //MEDIA INFO
    DOMElement * vsmplchElem = _doc->createElement(Xml("samplecharacteristics"));
    
    DOMElement * widthElem = _doc->createElement(Xml("width"));
    widthElem->appendChild(_doc->createTextNode(Xml(_width.c_str())));
    DOMElement * heightElem = _doc->createElement(Xml("height"));
    heightElem->appendChild(_doc->createTextNode(Xml(_height.c_str())));



    DOMElement * ismasterElem = _doc->createElement(Xml("ismasterclip"));
    ismasterElem->appendChild(_doc->createTextNode(Xml("TRUE")));

    st_time = getStartTime(topPackage, packages, g_palEditRate); //start frames from midnite

    Timecode tc(st_time);  //returns true time code

    //Rate details: Timecode, Display Format, Source
    DOMElement * tcElem = _doc->createElement(Xml("string"));
    tcElem->appendChild(_doc->createTextNode(Xml(tc.Text())));

    DOMElement * dfElem = _doc->createElement(Xml("displayformat"));
    dfElem->appendChild(_doc->createTextNode(Xml(df)));
    
    DOMElement * scElem = _doc->createElement(Xml("source"));
    scElem->appendChild(_doc->createTextNode(Xml(src)));

    //Start of DOM
    //name
    clipElem->appendChild(nameElem);
    //duration
    clipElem->appendChild(durElem1);

    //anamorphic
    clipElem->appendChild(anaElem);
    //rate
    clipElem->appendChild(rateElem);

    //file name, tc, path into file
    fileElem->appendChild(fnameElem);
    fileElem->appendChild(purlElem);
    fileElem->appendChild(tcElem);
    //file into clip
    fileElem->setAttribute(Xml("id"), Xml(topPackage->name.c_str()));
    clipElem->appendChild(fileElem);

    //timecode inside file
    timecodeElem->appendChild(tcElem);
    timecodeElem->appendChild(dfElem);
    timecodeElem->appendChild(scElem);
    fileElem->appendChild(timecodeElem);    
    int taudio=0;

    vector<prodauto::Track*>::const_iterator iter2;
    for (iter2 = topPackage->tracks.begin(); iter2 != topPackage->tracks.end(); iter2++) //looks in at Track Detail
    {
        prodauto::Track * track = *iter2;
        string trackname;
        string name = track->name.c_str();
        size_t pos = name.find_last_of("/");
        if(pos != string::npos)
        {
            trackname = name.substr(pos + 1);
        }
        else
        {
            trackname = name;
        }

        SourcePackage dummy;
        dummy.uid = track->sourceClip->sourcePackageUID;
        PackageSet::iterator result = packages.find(&dummy);

        if (result != packages.end())
        {
            Package* package = *result;
            if (package->getType() != SOURCE_PACKAGE || package->tracks.size() == 0)
            {
                continue;
            }
            
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            
            if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
            {
                continue;
            }
            FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
            prodauto::Track * track = *iter2;
            string filelocation;
            string floc = fileDescriptor->fileLocation.c_str();
            size_t pos = floc.find_last_of("/");
	    if(pos != string::npos)
            {
                filelocation = floc.substr(pos + 1);
            }

            else
            {
                filelocation = floc;
            }


            //File Name and Location
            if (track->dataDef == PICTURE_DATA_DEFINITION) //VIDEO INFO
            {
                //Length
                ostringstream dur_ss;
                dur_ss << track->sourceClip->length;        
                durElem->appendChild(_doc->createTextNode(Xml(dur_ss.str().c_str())));
                durElem1->appendChild(_doc->createTextNode(Xml(dur_ss.str().c_str())));
                purlElem->appendChild(_doc->createTextNode(Xml(_fcpPath.c_str())));
                purlElem->appendChild(_doc->createTextNode(Xml(filelocation.c_str())));
                fnameElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str())));
                //duration into file->media->video
                videoElem->appendChild(durElem);
                //width and height into video sample characterstics
                vsmplchElem->appendChild(widthElem);
                vsmplchElem->appendChild(heightElem);               
                //video sample characteristics into file->media->video
                videoElem->appendChild(vsmplchElem);
                //video into media
                mediaElem->appendChild(videoElem);
            }//Video
        
            else if (track->dataDef == SOUND_DATA_DEFINITION) //AUDIO INFO
            {
                taudio++;
	        DOMElement * audioElem = _doc->createElement(Xml("audio")); //Audio Tag
	        DOMElement * smplrteElem = _doc->createElement(Xml("samplerate"));
	        smplrteElem->appendChild(_doc->createTextNode(Xml("48000")));
	        DOMElement * dpthElem = _doc->createElement(Xml("depth"));
	        dpthElem->appendChild(_doc->createTextNode(Xml("16")));
	        DOMElement * layoutElem = _doc->createElement(Xml("layout"));
	        layoutElem->appendChild(_doc->createTextNode(Xml("stereo")));
	        DOMElement * asmplchElem = _doc->createElement(Xml("samplecharacteristics")); 
//	        DOMElement * chnllbrElem = _doc->createElement(Xml("channellabel"));
//	        DOMElement * audiochrElem1 = _doc->createElement(Xml("audiochannel"));
	      
	        //samplerate + depth into sample characteristics
	        asmplchElem->appendChild(smplrteElem);
	        asmplchElem->appendChild(dpthElem);   
	      
	        //audio sample characteristics into file->media->audio
	        audioElem->appendChild(asmplchElem);
	        //layout into audio
	        audioElem->appendChild(layoutElem);
	      
		//audio channel into file->media->audio
		DOMElement * chnlElem = _doc->createElement(Xml("channelcount"));
	        chnlElem->appendChild(_doc->createTextNode(Xml("1")));     
	        audioElem->appendChild(chnlElem); 			  
		
	        //audio channel label into audio channel
//	        ostringstream t1_ss;
//	        t1_ss << taudio;
//	        chnllbrElem->appendChild(_doc->createTextNode(Xml("ch")));
//	        chnllbrElem->appendChild(_doc->createTextNode(Xml(t1_ss.str().c_str())));
//	        audiochrElem1->appendChild(chnllbrElem);
//	        audioElem->appendChild(audiochrElem1);
	      
	        mediaElem->appendChild(audioElem);
             }//Audio
	 }//Packages
     } //Tracks

     
     
     */
     //
     // PART4 COMMENTS
     //
     
     int com = 0;
     // Locators (User Comments)
     vector<UserComment> mergedComments;
     vector<UserComment> userComments = materialPackage->getUserComments();
     DOMElement * MarkerElem = _doc->createElement(Xml("marker"));
     DOMElement * CommentElem = _doc->createElement(Xml("comments"));
     vector<UserComment>::const_iterator commentIter;
     for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
     {
         const UserComment& userComment = *commentIter;
	 if(userComment.position >= 0)
	 {
	     mergeLocatorUserComment(mergedComments, userComment);
         }
	 if (userComment.position != 0)
	 {
	     // marker's children
	     DOMElement * MarkerNameElem = _doc->createElement(Xml("name"));
	     DOMElement * MarkerCommentElem = _doc->createElement(Xml("comment"));
	     DOMElement * MarkerInElem = _doc->createElement(Xml("in"));
	     DOMElement * MarkerOutElem = _doc->createElement(Xml("out"));
		
	     MarkerNameElem->appendChild(_doc->createTextNode(Xml(userComment.name.c_str())));
	     MarkerCommentElem->appendChild(_doc->createTextNode(Xml(userComment.value.c_str())));
		
	     ostringstream inPos_ss;
	     inPos_ss << userComment.position;
		
	     MarkerInElem->appendChild(_doc->createTextNode(Xml(inPos_ss.str().c_str())));
	     MarkerOutElem->appendChild(_doc->createTextNode(Xml("-1")));
		
	     if (userComment.position > 0)
	     {
	         MarkerElem->appendChild(MarkerNameElem);
		 MarkerElem->appendChild(MarkerInElem);
		 MarkerElem->appendChild(MarkerOutElem);
		 MarkerElem->appendChild(MarkerCommentElem);				
		 mediaElem->appendChild(MarkerElem);
		    
	     } 
	     else 
	     {
		    
	         if (com >= 4)
		 {	
		     continue;	    
		 } 
		 else 
		 {
		     com++;
		     ostringstream com_ss;
		     com_ss << com;
		    
		     std::string masterCom = "mastercomment" + com_ss.str();
		     DOMElement * MCommentElem = _doc->createElement(Xml(masterCom.c_str()));
		    
		     MCommentElem->appendChild(_doc->createTextNode(Xml(userComment.value.c_str())));
		     CommentElem->appendChild(MCommentElem);
		     clipElem->appendChild(CommentElem);		
	         }
	     }
	 }
     }

     //
     //default angle, must increase by 1 for each clip
     //
     DOMElement * defangleElem = _doc->createElement(Xml("defaultangle"));
     defangleElem->appendChild(_doc->createTextNode(Xml(angle_ss.str().c_str())));
     clipElem->appendChild(defangleElem);   
     
  /*
//     ostringstream taudioss;
//     taudioss << taudio;    
     //channel count into audio
//     chnlElem->appendChild(_doc->createTextNode(Xml(taudioss.str().c_str())));
//     audioElem->appendChild(chnlElem);  
     //media into file
     fileElem->appendChild(mediaElem);
     //clip details cntd.
     clipElem->appendChild(inElem);
     clipElem->appendChild(outElem);
     //master clip id, name plus M
     DOMElement * masteridElem = _doc->createElement(Xml("masterclipid"));
     masteridElem->appendChild(_doc->createTextNode(Xml("M")));
     masteridElem->appendChild(_doc->createTextNode(Xml(topPackage->name.c_str())));
     clipElem->appendChild(masteridElem);
     clipElem->appendChild(ismasterElem);
     //default angle, must increase by 1 for each clip
     DOMElement * defangleElem = _doc->createElement(Xml("defaultangle"));
     defangleElem->appendChild(_doc->createTextNode(Xml(angle_ss.str().c_str())));
     clipElem->appendChild(defangleElem);   
    
     
     */
     	// #endif
	// End of Clip definitions
}


bool FCPFile::addMCClip(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,vector<CutInfo> sequence)
{
    bool mcClipWasAdded;
    
    mcClipWasAdded = addMCClipInternal(mcClipDef, materialPackages, packages);
    
    if (!sequence.empty())
    {
        addMCSequenceInternal(mcClipDef, materialPackages, packages, sequence);
    }

    return mcClipWasAdded;
}

bool FCPFile::addMCClipInternal(MCClipDef* itmcP, MaterialPackageSet& materialPackages, PackageSet& packages)
{
    //settingsStruct *settings;
    
    MaterialPackageSet::const_iterator iterSet = materialPackages.begin();
    MaterialPackage* topPackageSet = *iterSet;
    getSettings(topPackageSet, packages);
    
    bool mcClipWasAdded = false;//for final test
    
    
#if TESTING_FLAG
    // multi-camera clip and director's cut sequence
  //  static const char * ntsc = "FALSE";
  //  static const char * tb ="25";
    int in= -1;
    int out= -1;
    // load multi-camera clip definitions- need MCClip defs    
    Database* database = Database::getInstance();
    VectorGuard<MCClipDef> mcClipDefs;
    mcClipDefs.get() = database->loadAllMultiCameraClipDefs();
    //Media Tag
    DOMElement * mediaElem = _doc->createElement(Xml("media"));
    vector<MCClipDef*>::iterator itmc;
    DOMElement * _rootElem = _doc->getDocumentElement();
    //Mclip
    DOMElement * MclipElem = _doc->createElement(Xml("clip"));
    int c1 = 0;
    
    //video
 
    ostringstream idname_ss;
    idname_ss << _idName;
    
    
    ostringstream fR_ss;
    fR_ss << _frameRate;
    //DOM Details for Multiclip
    //TAGS
    //Rate Tag
    DOMElement * rateElem = _doc->createElement(Xml("rate"));
    DOMElement * rate2Elem = _doc->createElement(Xml("rate"));
    //Video Tag
    DOMElement * videoElem = _doc->createElement(Xml("video"));
    //rate
    DOMElement * ntscElem = _doc->createElement(Xml("ntsc"));
    ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
    DOMElement * tbElem = _doc->createElement(Xml("timebase"));
    tbElem->appendChild(_doc->createTextNode(Xml(fR_ss.str().c_str())));
    rateElem->appendChild(ntscElem);
    rateElem->appendChild(tbElem);
    DOMElement * ntscElem2 = _doc->createElement(Xml("ntsc"));
    ntscElem2->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
    DOMElement * tbElem2 = _doc->createElement(Xml("timebase"));
    tbElem2->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
    rate2Elem->appendChild(ntscElem2);
    rate2Elem->appendChild(tbElem2);
    //Mduration
    DOMElement * MdurElem = _doc->createElement(Xml("duration"));
    DOMElement * Mdur2Elem = _doc->createElement(Xml("duration"));
    //Mclipitem1
    DOMElement * Mclipitem1Elem = _doc->createElement(Xml("clipitem"));
    //Multiclip id
    DOMElement * MclipidElem = _doc->createElement(Xml("multiclip"));
    //Mcollapsed
    DOMElement * McollapsedElem = _doc->createElement(Xml("collapsed"));
    //Msynctype
    DOMElement * MsyncElem = _doc->createElement(Xml("synctype"));
    //Mname
    DOMElement * MnameElem = _doc->createElement(Xml("name"));
    DOMElement * Mname1Elem = _doc->createElement(Xml("name"));
    DOMElement * Mname2Elem = _doc->createElement(Xml("name"));
    //Track Tag
    DOMElement * trackElem = _doc->createElement(Xml("track"));
    DOMElement * inElem = _doc->createElement(Xml("in"));
    ostringstream in_ss;
    in_ss << in;
    inElem->appendChild(_doc->createTextNode(Xml(in_ss.str().c_str())));
    DOMElement * outElem = _doc->createElement(Xml("out"));
    ostringstream out_ss;
    out_ss << out;
    outElem->appendChild(_doc->createTextNode(Xml(out_ss.str().c_str())));
    DOMElement * ismasterElem = _doc->createElement(Xml("ismasterclip"));
    ismasterElem->appendChild(_doc->createTextNode(Xml("FALSE")));

    MaterialPackageSet::const_iterator iter1;
    for (iter1 = materialPackages.begin(); iter1 != materialPackages.end(); iter1++)
    {   
        //Mclip
        DOMElement * Mclip2Elem = _doc->createElement(Xml("clip"));
        //Mclip
        DOMElement * MfileElem = _doc->createElement(Xml("file"));
        //Mangle1
        DOMElement * Mangle1Elem = _doc->createElement(Xml("angle"));
        //Start of code::
        MaterialPackage* topPackage1 = *iter1;
        MaterialPackageSet materialPackages;
        materialPackages.insert(topPackage1);
        //DOM Multicam 
        // add material to group that has same start time and creation date
        int64_t st_time;
        vector<prodauto::Track*>::const_iterator iter6;
        for (iter6 = topPackage1->tracks.begin(); iter6 != topPackage1->tracks.end(); iter6++) 
        {
            prodauto::Track * track = *iter6;
            SourcePackage dummy;
            dummy.uid = track->sourceClip->sourcePackageUID;
            PackageSet::iterator result = packages.find(&dummy);
            vector<MCClipDef*>::iterator itmc1;
            for (itmc1 = mcClipDefs.get().begin(); itmc1 != mcClipDefs.get().end(); itmc1++)
            {
                MCClipDef * mcClipDef = itmcP;              
                if ((*itmc1)->name == itmcP->name)
                {
                    vector<SourceConfig*>::const_iterator itmc2;
                    for (itmc2 = mcClipDef->sourceConfigs.begin(); itmc2 != mcClipDef->sourceConfigs.end(); itmc2++)
                    {
                        Package* package = *result;
                        SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
                        if ((*itmc2)->name == sourcePackage->sourceConfigName)
                        {
                            c1++;
                            if (c1 == 1)
                            {
                                //MClip DOM Start
                                _rootElem->appendChild(MclipElem);
                                MclipElem->setAttribute(Xml("id"), Xml(idname_ss.str().c_str()));
                                //Name
                                Mname2Elem->appendChild(_doc->createTextNode(Xml("Multicam")));
                                Mname2Elem->appendChild(_doc->createTextNode(Xml(idname_ss.str().c_str())));
                                Mname1Elem->appendChild(_doc->createTextNode(Xml("Multicam")));
                                Mname1Elem->appendChild(_doc->createTextNode(Xml(idname_ss.str().c_str())));
                                MnameElem->appendChild(_doc->createTextNode(Xml("Multicam")));
                                MnameElem->appendChild(_doc->createTextNode(Xml(idname_ss.str().c_str())));
                                //Duration
    
                                ostringstream tMdurss;
                                tMdurss << track->sourceClip->length;
                                MdurElem->appendChild(_doc->createTextNode(Xml(tMdurss.str().c_str())));
                                Mdur2Elem->appendChild(_doc->createTextNode(Xml(tMdurss.str().c_str())));
                                //Clip Items: name
                                Mclipitem1Elem->setAttribute(Xml("id"), Xml(idname_ss.str().c_str()));
                                //Collapsed
                                McollapsedElem->appendChild(_doc->createTextNode(Xml("FALSE")));
                                //Sync Type
                                //Append above to Tracks
                                MclipElem->appendChild(Mname1Elem);
                                MclipElem->appendChild(MdurElem);
                                //Rate
                                MclipElem->appendChild(rateElem);
                                MclipElem->appendChild(inElem);
                                MclipElem->appendChild(outElem);
                                MclipElem->appendChild(ismasterElem);
                                //Multiclip
    
                                Mclipitem1Elem->appendChild(Mname2Elem);
                                Mclipitem1Elem->appendChild(Mdur2Elem);
                                Mclipitem1Elem->appendChild(rate2Elem);
                                Mclipitem1Elem->setAttribute(Xml("id"), Xml(idname_ss.str().c_str()));
                                MclipidElem->appendChild(MnameElem);
                                MclipidElem->appendChild(McollapsedElem);
                                MclipidElem->appendChild(MsyncElem);
                                MclipidElem->setAttribute(Xml("id"), Xml(idname_ss.str().c_str()));
                            }
                            st_time = getStartTime(topPackage1, packages, g_palEditRate);
                            Timecode tc(st_time);
                            if (track->dataDef == PICTURE_DATA_DEFINITION) //VIDEO
                            { 
                                Mclip2Elem->setAttribute(Xml("id"), Xml(topPackage1->name.c_str()));    
                                MfileElem->setAttribute(Xml("id"), Xml(topPackage1->name.c_str()));
                                Mclip2Elem->appendChild(MfileElem);       
                                Mangle1Elem->appendChild(Mclip2Elem);
                                MclipidElem->appendChild(Mangle1Elem);
                                Mclipitem1Elem->appendChild(MclipidElem);
                                trackElem->appendChild(Mclipitem1Elem);
                                videoElem->appendChild(trackElem);
                                mediaElem->appendChild(videoElem);
                                mcClipWasAdded = true;
                            }
                        }
                    }
                }
            }
        }

	// Locators (User Comments)
	vector<UserComment> userComments = mergeUserComments(materialPackages);
	vector<UserComment>::iterator commentIter;
	// markers
	DOMElement * MarkerElem = _doc->createElement(Xml("marker"));
	for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
	{
		UserComment& userComment = *commentIter;
		if (userComment.position != 0)
		{
			// marker's children
			DOMElement * MarkerNameElem = _doc->createElement(Xml("name"));
			DOMElement * MarkerCommentElem = _doc->createElement(Xml("comment"));
			DOMElement * MarkerInElem = _doc->createElement(Xml("in"));
			DOMElement * MarkerOutElem = _doc->createElement(Xml("out"));
		
			MarkerNameElem->appendChild(_doc->createTextNode(Xml(userComment.name.c_str())));
			MarkerCommentElem->appendChild(_doc->createTextNode(Xml(userComment.value.c_str())));
			
			ostringstream inPos_ss;
			inPos_ss << userComment.position;
			MarkerInElem->appendChild(_doc->createTextNode(Xml(inPos_ss.str().c_str())));
			
			MarkerOutElem->appendChild(_doc->createTextNode(Xml("-1")));
			
			MarkerElem->appendChild(MarkerNameElem);
			MarkerElem->appendChild(MarkerInElem);
			MarkerElem->appendChild(MarkerOutElem);
			MarkerElem->appendChild(MarkerCommentElem);
			
			MclipElem->appendChild(MarkerElem);
		}
	}

    }//video
    MclipElem->appendChild(mediaElem);
    {//audio
        int taudio =0;
        DOMElement * audioElem = _doc->createElement(Xml("audio"));
        map<uint32_t, MCTrackDef*>::const_iterator itMC2;
        for (itMC2 = itmcP->trackDefs.begin(); itMC2 != itmcP->trackDefs.end(); itMC2++)
        {
            MCTrackDef* trackDef = (*itMC2).second;
            for ( map<uint32_t, MCSelectorDef*>::const_iterator itMC3 = trackDef->selectorDefs.begin(); itMC3 != trackDef->selectorDefs.end(); itMC3++)
            {
                MCSelectorDef* selectorDef = (*itMC3).second;
                SourceTrackConfig* trackConfig =  selectorDef->sourceConfig->getTrackConfig(selectorDef->sourceTrackID);
                if (trackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    MaterialPackageSet::const_iterator iter2;
                    for (iter2 = materialPackages.begin(); iter2 != materialPackages.end(); iter2++)
                    {
                        MaterialPackage* topPackage2 = *iter2;
                        vector<prodauto::Track*>::const_iterator iter6;
                        for (iter6 = topPackage2->tracks.begin(); iter6 != topPackage2->tracks.end(); iter6 ++)
                        {
                            prodauto::Track * track = *iter6;
                            SourcePackage* fileSourcePackage;
                            SourcePackage* sourcePackage;
                            Track* sourceTrack;
                            if (! getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                            {
                                continue;
                            }
                            if (sourceTrack->dataDef == SOUND_DATA_DEFINITION)
                            {
                                if (selectorDef->sourceConfig->name == fileSourcePackage->sourceConfigName && selectorDef->sourceTrackID == sourceTrack->id)
                                {   
                                    taudio++;
                                    if (taudio==1)
                                    {
                                        DOMElement * AformatElem = _doc->createElement(Xml("format"));
                                        DOMElement * AsmpcharElem = _doc->createElement(Xml("samplecharacteristics"));
                                        DOMElement * AdepthElem = _doc->createElement(Xml("depth"));
                                        AdepthElem->appendChild(_doc->createTextNode(Xml("16")));
                                        DOMElement * AsmprateElem = _doc->createElement(Xml("samplerate"));
                                        AsmprateElem->appendChild(_doc->createTextNode(Xml("48000")));
                                        AsmpcharElem->appendChild(AdepthElem);
                                        AsmpcharElem->appendChild(AsmprateElem);
                                        AformatElem->appendChild(AsmpcharElem);
                                        DOMElement * AoutputsElem = _doc->createElement(Xml("outputs"));
                                        DOMElement * AgroupElem = _doc->createElement(Xml("group"));
                                        DOMElement * AchannelElem = _doc->createElement(Xml("channel"));
                                        DOMElement * AindexElem = _doc->createElement(Xml("index"));
                                        AindexElem->appendChild(_doc->createTextNode(Xml("1")));
                                        DOMElement * AnumchElem = _doc->createElement(Xml("numchannels"));
                                        AnumchElem->appendChild(_doc->createTextNode(Xml("2")));
                                        DOMElement * AdownmixElem = _doc->createElement(Xml("downmix"));
                                        AdownmixElem->appendChild(_doc->createTextNode(Xml("2")));
                                        DOMElement * ACindex1Elem = _doc->createElement(Xml("index"));
                                        DOMElement * ACindex2Elem = _doc->createElement(Xml("index"));
                                        AgroupElem->appendChild(AindexElem);
                                        AgroupElem->appendChild(AnumchElem);
                                        AgroupElem->appendChild(AdownmixElem);
                                        ACindex1Elem->appendChild(_doc->createTextNode(Xml("1")));
                                        AchannelElem->appendChild(ACindex1Elem);
                                        AgroupElem->appendChild(AchannelElem);
                                        ACindex2Elem->appendChild(_doc->createTextNode(Xml("2")));
                                        AchannelElem->appendChild(ACindex2Elem);
                                        AgroupElem->appendChild(AchannelElem);
                                        AoutputsElem->appendChild(AgroupElem);
                                        audioElem->appendChild(AformatElem);
                                        audioElem->appendChild(AoutputsElem); 
                                    }
                                    DOMElement * Asource2Elem = _doc->createElement(Xml("sourcetrack"));
                                    DOMElement * Amedia2Elem = _doc->createElement(Xml("mediatype"));
                                    Amedia2Elem->appendChild(_doc->createTextNode(Xml("audio")));
                                    ostringstream ta_ss;
                                    ta_ss << taudio;
                                    DOMElement * AtrackindexElem = _doc->createElement(Xml("trackindex"));
                                    AtrackindexElem->appendChild(_doc->createTextNode(Xml(ta_ss.str().c_str())));
                                    DOMElement * AtrackElem = _doc->createElement(Xml("track"));
                                    DOMElement * AclipitemElem = _doc->createElement(Xml("clipitem"));
                                    AclipitemElem->setAttribute(Xml("id"), Xml(fileSourcePackage->sourceConfigName.c_str()));
                                    DOMElement * AfileElem = _doc->createElement(Xml("file"));
                                    AfileElem->setAttribute(Xml("id"), Xml(topPackage2->name.c_str()));
                                    DOMElement * Astart2Elem = _doc->createElement(Xml("start"));
                                    Astart2Elem->appendChild(_doc->createTextNode(Xml("0")));
                                    DOMElement * Aend2Elem = _doc->createElement(Xml("end"));
                                    int Length = trackConfig->length / 48000;
                                    ostringstream le_ss;
                                    le_ss << Length;
                                    Aend2Elem->appendChild(_doc->createTextNode(Xml(le_ss.str().c_str())));
                                    Asource2Elem->appendChild(Amedia2Elem);
                                    Asource2Elem->appendChild(AtrackindexElem);
                                    AclipitemElem->appendChild(AfileElem);
                                    AclipitemElem->appendChild(Asource2Elem);
                                    AclipitemElem->appendChild(Astart2Elem);
                                    AclipitemElem->appendChild(Aend2Elem);
                                    AtrackElem->appendChild(AclipitemElem);
                                    audioElem->appendChild(AtrackElem);
                        
                                }   
                            }
                        }
                    }
                }
            }
        }
        mediaElem->appendChild(audioElem);
    }//audio
    
    if (mcClipWasAdded)
    {
        _idName++;
    }

    
#endif
    return mcClipWasAdded;
  
}

// add a multi-camera clip sequence from Directors Cut
void FCPFile::addMCSequenceInternal(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,vector<CutInfo> sequence)
{
#if TESTING_FLAG
    MaterialPackageSet::const_iterator iterSet = materialPackages.begin();
    MaterialPackage* topPackageSet = *iterSet;	
    //settingsStruct *settings;
    getSettings(topPackageSet, packages);
    
    
    ostringstream fR_ss;
    fR_ss << _frameRate;
    
    int end = 0;
    Database* database = Database::getInstance();
    VectorGuard<MCClipDef> mcClipDefs;
    mcClipDefs.get() = database->loadAllMultiCameraClipDefs();

    vector<MCClipDef*>::iterator itMC;
  //  static const char * ntsc = "FALSE";
  //  static const char * tb ="25";

    //DOM DOC
    DOMElement * _rootElem = _doc->getDocumentElement();

    //TAGS
    //Rate Tag
    DOMElement * rateElem = _doc->createElement(Xml("rate"));
    //Rate Tag
    DOMElement * rate2Elem = _doc->createElement(Xml("rate"));
    //Media Tag 
    DOMElement * mediaElem = _doc->createElement(Xml("media"));
    //Video Tag
    DOMElement * videoElem = _doc->createElement(Xml("video"));
    //Duration
    DOMElement *durElem = _doc->createElement(Xml("duration"));
    //rate
    DOMElement * ntscElem = _doc->createElement(Xml("ntsc"));
    ntscElem->appendChild(_doc->createTextNode(Xml(_ntsc.c_str())));
    //set rate properties
    DOMElement * tbElem = _doc->createElement(Xml("timebase"));
    tbElem->appendChild(_doc->createTextNode(Xml(fR_ss.str().c_str())));
    rateElem->appendChild(ntscElem);
    rateElem->appendChild(tbElem);
 
    //set rate properties
    DOMElement * tb2Elem = _doc->createElement(Xml("timebase"));
    tb2Elem->appendChild(_doc->createTextNode(Xml(fR_ss.str().c_str())));
    rate2Elem->appendChild(tb2Elem);

    //Open XML Clip
    DOMElement * seqElem = _doc->createElement(Xml("sequence"));
    _rootElem->appendChild(seqElem);
    //Sequence Name
    DOMElement * nameElem = _doc->createElement(Xml("name"));
    nameElem->appendChild(_doc->createTextNode(Xml("Directors Cut Sequence")));
    //Sequence ID    
    seqElem->setAttribute(Xml("id"), Xml("Directors Cut Sequence"));


    //<format>
    DOMElement * formatElem = _doc->createElement(Xml("format"));
    //<sample charactistics>
    DOMElement * widthElem = _doc->createElement(Xml("width"));
    widthElem->appendChild(_doc->createTextNode(Xml(_width.c_str())));
    DOMElement * heightElem = _doc->createElement(Xml("height"));
    heightElem->appendChild(_doc->createTextNode(Xml(_height.c_str())));
    DOMElement * pixElem = _doc->createElement(Xml("pixelaspectratio"));
    pixElem->appendChild(_doc->createTextNode(Xml(_pixelAspect.c_str())));
    DOMElement * fieldElem = _doc->createElement(Xml("fielddominance"));
    fieldElem->appendChild(_doc->createTextNode(Xml(_fieldDominance.c_str())));
    DOMElement * sampleElem = _doc->createElement(Xml("samplecharacteristics"));
    sampleElem->appendChild(widthElem);
    sampleElem->appendChild(heightElem);
    sampleElem->appendChild(pixElem);
    sampleElem->appendChild(fieldElem);
    sampleElem->appendChild(rateElem);

    //<codec info>
    DOMElement * codecElem = _doc->createElement(Xml("codec"));
    DOMElement * codecNameElem = _doc->createElement(Xml("name"));
    codecNameElem->appendChild(_doc->createTextNode(Xml(_codecName.c_str())));
    DOMElement * appElem = _doc->createElement(Xml("appspecificdata"));
    DOMElement * appNameElem = _doc->createElement(Xml("appname"));
    appNameElem->appendChild(_doc->createTextNode(Xml(_appName.c_str())));
    DOMElement * appManElem = _doc->createElement(Xml("appmanufacturer"));
    appManElem->appendChild(_doc->createTextNode(Xml(_appManufacturer.c_str())));
    DOMElement * appVSElem = _doc->createElement(Xml("appversion"));
    appVSElem->appendChild(_doc->createTextNode(Xml(_appVersion.c_str())));
    DOMElement * dataElem = _doc->createElement(Xml("data"));
    DOMElement * qtcodecElem = _doc->createElement(Xml("qtcodec"));
    DOMElement * qtcodecNameElem = _doc->createElement(Xml("codecname"));
    qtcodecNameElem->appendChild(_doc->createTextNode(Xml(_codecName.c_str())));
    DOMElement * qtcodectypeElem = _doc->createElement(Xml("codectypename"));
    qtcodectypeElem->appendChild(_doc->createTextNode(Xml(_codecType.c_str())));
    DOMElement * qtcodecspElem = _doc->createElement(Xml("spatialquality"));
    qtcodecspElem->appendChild(_doc->createTextNode(Xml(_codecSpatial.c_str())));
    DOMElement * qtcodectpElem = _doc->createElement(Xml("temporalquality"));
    qtcodectpElem->appendChild(_doc->createTextNode(Xml(_codecTemporal.c_str())));
    DOMElement * qtcodeckfElem = _doc->createElement(Xml("keyframerate"));
    qtcodeckfElem->appendChild(_doc->createTextNode(Xml(_codecKeyframe.c_str())));
    DOMElement * qtcodecdataElem = _doc->createElement(Xml("datarate"));
    qtcodecdataElem->appendChild(_doc->createTextNode(Xml(_codecDatarate.c_str())));
    qtcodecElem->appendChild(qtcodecNameElem);
    qtcodecElem->appendChild(qtcodectypeElem);
    qtcodecElem->appendChild(qtcodecspElem);
    qtcodecElem->appendChild(qtcodectpElem);
    qtcodecElem->appendChild(qtcodeckfElem);
    qtcodecElem->appendChild(qtcodecdataElem);
    dataElem->appendChild(qtcodecElem);
    appElem->appendChild(appNameElem);
    appElem->appendChild(appManElem);
    appElem->appendChild(appVSElem);
    appElem->appendChild(dataElem);
    codecElem->appendChild(codecNameElem);
    codecElem->appendChild(appElem);
    sampleElem->appendChild(codecElem);

    //<image processing info>
    DOMElement * appElem2 = _doc->createElement(Xml("appspecificdata"));
    DOMElement * appNameElem2 = _doc->createElement(Xml("appname"));
    appNameElem2->appendChild(_doc->createTextNode(Xml("Final Cut Pro")));
    DOMElement * appManElem2 = _doc->createElement(Xml("appmanufacturer"));
    appManElem2->appendChild(_doc->createTextNode(Xml("Apple Computer, Inc.")));
    DOMElement * appVSElem2 = _doc->createElement(Xml("appversion"));
    appVSElem2->appendChild(_doc->createTextNode(Xml("5.0")));
    DOMElement * dataElem2 = _doc->createElement(Xml("data"));
    DOMElement * fcpimageElem = _doc->createElement(Xml("fcpimageprocessing"));
    DOMElement * useyuvElem = _doc->createElement(Xml("useyuv"));
    useyuvElem->appendChild(_doc->createTextNode(Xml("TRUE")));
    DOMElement * usewhiteElem = _doc->createElement(Xml("usesuperwhite"));
    usewhiteElem->appendChild(_doc->createTextNode(Xml("FALSE")));
    DOMElement * renderElem = _doc->createElement(Xml("rendermode"));
    renderElem->appendChild(_doc->createTextNode(Xml("YUV8BPP")));
    appElem2->appendChild(appNameElem2);
    appElem2->appendChild(appManElem2);
    appElem2->appendChild(appVSElem2);
    fcpimageElem->appendChild(useyuvElem);
    fcpimageElem->appendChild(usewhiteElem);
    fcpimageElem->appendChild(renderElem);
    dataElem2->appendChild(fcpimageElem);
    appElem2->appendChild(dataElem2);

    formatElem->appendChild(sampleElem);
    formatElem->appendChild(appElem2);
    //NOW ONTO TRACKS
    DOMElement * trackElem = _doc->createElement(Xml("track"));
    int cut = 0;

    int poss = 1;

    vector<CutInfo>::const_iterator it;
    for (it = sequence.begin(); it != sequence.end(); ++it)
    {   
        MaterialPackageSet::const_iterator iter2;
        for (iter2 = materialPackages.begin(); iter2 != materialPackages.end(); iter2++)
        {
            MaterialPackage* topPackage2 = *iter2;
            vector<prodauto::Track*>::const_iterator iter6 = topPackage2->tracks.begin();
            prodauto::Track * track = *iter6;
            SourcePackage dummy;
            dummy.uid = track->sourceClip->sourcePackageUID;
            PackageSet::iterator result = packages.find(&dummy);
            Package* package = *result;
            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
            if (sourcePackage->sourceConfigName == it->source)
            {
                printf("--------------------\n");
                printf("Sequence Database Infomation:\n");
                printf("Cut Number: %d\n", cut);
                printf("position %"PRIi64"\n",it->position);
                printf("video source %s\n",it->source.c_str()); 
                printf("Material Package Infomation:\n");
                printf("package name: %s\n",topPackage2->name.c_str());
                printf("Source Package Config name: %s\n",sourcePackage->sourceConfigName.c_str());
                printf("--------------------\n");
                printf("\n");
                //Video
                //Clip item Tag
		++cut;
                if (track->dataDef !=  SOUND_DATA_DEFINITION)
                {
                    DOMElement * clipitemElem = _doc->createElement(Xml("clipitem"));
                    clipitemElem->setAttribute(Xml("id"), Xml(it->source.c_str()));
                    DOMElement * fileElem = _doc->createElement(Xml("file"));
                    DOMElement * clipnameElem = _doc->createElement(Xml("name"));
                    clipnameElem->appendChild(_doc->createTextNode(Xml(it->source.c_str()))); 
                    fileElem->setAttribute(Xml("id"), Xml(topPackage2->name.c_str()));
                    DOMElement * inElem = _doc->createElement(Xml("in"));
                    DOMElement * outElem = _doc->createElement(Xml("out"));
                    outElem->appendChild(_doc->createTextNode(Xml("-1")));
                    DOMElement * startElem = _doc->createElement(Xml("start"));
                    ostringstream start_ss;
                    start_ss << it->position;
                    poss = it->position;
                    inElem->appendChild(_doc->createTextNode(Xml(start_ss.str().c_str())));
                    startElem->appendChild(_doc->createTextNode(Xml(start_ss.str().c_str())));
                    DOMElement * endElem = _doc->createElement(Xml("end"));
                    vector<CutInfo>::const_iterator it8 = it+1;
                    
                    if (it8 != sequence.end())
                    {
                        int poss2 = it8->position;
                        end = poss2;
                    }
                    if (it8 == sequence.end())
                    {
                        end = track->sourceClip->length;
                    }
                    ostringstream end_ss;
                    end_ss << end;
                    endElem->appendChild(_doc->createTextNode(Xml(end_ss.str().c_str())));
                    {
                        clipitemElem->appendChild(clipnameElem);
                        clipitemElem->appendChild(fileElem);
                        clipitemElem->appendChild(inElem);
                        clipitemElem->appendChild(outElem);
                        clipitemElem->appendChild(startElem);
                        clipitemElem->appendChild(endElem);
                        trackElem->appendChild(clipitemElem);
                    }           
                }
            }
        }
        printf("\n");
        }

	// Locators (User Comments)
	vector<UserComment> userComments = mergeUserComments(materialPackages);
	vector<UserComment>::iterator commentIter;
	// markers
	DOMElement * MarkerElem = _doc->createElement(Xml("marker"));
	for (commentIter = userComments.begin(); commentIter != userComments.end(); commentIter++)
	{
		UserComment& userComment = *commentIter;
		if (userComment.position != 0)
		{
			// marker's children
			DOMElement * MarkerNameElem = _doc->createElement(Xml("name"));
			DOMElement * MarkerCommentElem = _doc->createElement(Xml("comment"));
			DOMElement * MarkerInElem = _doc->createElement(Xml("in"));
			DOMElement * MarkerOutElem = _doc->createElement(Xml("out"));
		
			MarkerNameElem->appendChild(_doc->createTextNode(Xml(userComment.name.c_str())));
			MarkerCommentElem->appendChild(_doc->createTextNode(Xml(userComment.value.c_str())));
			
			ostringstream inPos_ss;
			inPos_ss << userComment.position;
			MarkerInElem->appendChild(_doc->createTextNode(Xml(inPos_ss.str().c_str())));
			
			MarkerOutElem->appendChild(_doc->createTextNode(Xml("-1")));
			
			MarkerElem->appendChild(MarkerNameElem);
			MarkerElem->appendChild(MarkerInElem);
			MarkerElem->appendChild(MarkerOutElem);
			MarkerElem->appendChild(MarkerCommentElem);
			
			videoElem->appendChild(MarkerElem);
		}
	}

        videoElem->appendChild(formatElem);
        videoElem->appendChild(trackElem);
        mediaElem->appendChild(videoElem);
//AUDIO
    int taudio =0;
    DOMElement * audioElem = _doc->createElement(Xml("audio"));
    {
        map<uint32_t, MCTrackDef*>::const_iterator itMC2;
        for (itMC2 = mcClipDef->trackDefs.begin(); itMC2 != mcClipDef->trackDefs.end(); itMC2++)
        {
            MCTrackDef* trackDef = (*itMC2).second;
            map<uint32_t, MCSelectorDef*>::const_iterator itMC3;
            for (itMC3 = trackDef->selectorDefs.begin(); itMC3 != trackDef->selectorDefs.end(); itMC3++)
            {
                MCSelectorDef* selectorDef = (*itMC3).second;
                SourceTrackConfig* trackConfig =  selectorDef->sourceConfig->getTrackConfig(selectorDef->sourceTrackID);
                if (trackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    printf("*****source track config name %s\n", trackConfig->name.c_str());
                    MaterialPackageSet::const_iterator iter2;
                    for (iter2 = materialPackages.begin(); iter2 != materialPackages.end(); iter2++)
                    {
                        MaterialPackage* topPackage2 = *iter2;
                        vector<prodauto::Track*>::const_iterator iter6;
                        for (iter6 = topPackage2->tracks.begin(); iter6 != topPackage2->tracks.end(); iter6 ++)
                        {
                            prodauto::Track * track = *iter6;
                            SourcePackage* fileSourcePackage;
                            SourcePackage* sourcePackage;
                            Track* sourceTrack;
                            if (! getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                            {
                                continue;
                            }
                            if (sourceTrack->dataDef == SOUND_DATA_DEFINITION)
                            {
                                if (selectorDef->sourceConfig->name == fileSourcePackage->sourceConfigName && selectorDef->sourceTrackID == sourceTrack->id)
                                {   
                                    taudio++;
                                    if (taudio==1)
                                    {
                                        DOMElement * AformatElem = _doc->createElement(Xml("format"));
                                        DOMElement * AsmpcharElem = _doc->createElement(Xml("samplecharacteristics"));
                                        DOMElement * AdepthElem = _doc->createElement(Xml("depth"));
                                        AdepthElem->appendChild(_doc->createTextNode(Xml("16")));
                                        DOMElement * AsmprateElem = _doc->createElement(Xml("samplerate"));
                                        AsmprateElem->appendChild(_doc->createTextNode(Xml("48000")));
                                        AsmpcharElem->appendChild(AdepthElem);
                                        AsmpcharElem->appendChild(AsmprateElem);
                                        AformatElem->appendChild(AsmpcharElem);
                                        DOMElement * AoutputsElem = _doc->createElement(Xml("outputs"));
                                        DOMElement * AgroupElem = _doc->createElement(Xml("group"));
                                        DOMElement * AchannelElem = _doc->createElement(Xml("channel"));
                                        DOMElement * AindexElem = _doc->createElement(Xml("index"));
                                        AindexElem->appendChild(_doc->createTextNode(Xml("1")));
                                        DOMElement * AnumchElem = _doc->createElement(Xml("numchannels"));
                                        AnumchElem->appendChild(_doc->createTextNode(Xml("2")));
                                        DOMElement * AdownmixElem = _doc->createElement(Xml("downmix"));
                                        AdownmixElem->appendChild(_doc->createTextNode(Xml("2")));
                                        DOMElement * ACindex1Elem = _doc->createElement(Xml("index"));
                                        DOMElement * ACindex2Elem = _doc->createElement(Xml("index"));
                                        AgroupElem->appendChild(AindexElem);
                                        AgroupElem->appendChild(AnumchElem);
                                        AgroupElem->appendChild(AdownmixElem);
                                        ACindex1Elem->appendChild(_doc->createTextNode(Xml("1")));
                                        AchannelElem->appendChild(ACindex1Elem);
                                        AgroupElem->appendChild(AchannelElem);
                                        ACindex2Elem->appendChild(_doc->createTextNode(Xml("2")));
                                        AchannelElem->appendChild(ACindex2Elem);
                                        AgroupElem->appendChild(AchannelElem);
                                        AoutputsElem->appendChild(AgroupElem);
                                        audioElem->appendChild(AformatElem);
                                        audioElem->appendChild(AoutputsElem);
                                    }
                                    printf("file id= %s\n",topPackage2->name.c_str());
                                    printf("number of tracks =%d\n", taudio);
                                    printf("Source Package Config name: %s\n",fileSourcePackage->sourceConfigName.c_str()); 
                                    DOMElement * Asource2Elem = _doc->createElement(Xml("sourcetrack"));
                                    DOMElement * Amedia2Elem = _doc->createElement(Xml("mediatype"));
                                    Amedia2Elem->appendChild(_doc->createTextNode(Xml("audio")));
                                    ostringstream ta_ss;
                                    ta_ss << taudio;
                                    DOMElement * AtrackindexElem = _doc->createElement(Xml("trackindex"));
                                    AtrackindexElem->appendChild(_doc->createTextNode(Xml(ta_ss.str().c_str())));
                                    DOMElement * AtrackElem = _doc->createElement(Xml("track"));
                                    DOMElement * AclipitemElem = _doc->createElement(Xml("clipitem"));
                                    AclipitemElem->setAttribute(Xml("id"), Xml("Audio"));
                                    DOMElement * AfileElem = _doc->createElement(Xml("file"));
                                    AfileElem->setAttribute(Xml("id"), Xml(topPackage2->name.c_str()));
                                    DOMElement * Astart2Elem = _doc->createElement(Xml("start"));
                                    Astart2Elem->appendChild(_doc->createTextNode(Xml("0")));
                                    DOMElement * Aend2Elem = _doc->createElement(Xml("end"));
                                    int Length = end;
                                    ostringstream le_ss;
                                    le_ss << Length;
                                    Aend2Elem->appendChild(_doc->createTextNode(Xml(le_ss.str().c_str())));
                                    Asource2Elem->appendChild(Amedia2Elem);
                                    Asource2Elem->appendChild(AtrackindexElem);
                                    AclipitemElem->appendChild(AfileElem);
                                    AclipitemElem->appendChild(Asource2Elem);
                                    AclipitemElem->appendChild(Astart2Elem);
                                    AclipitemElem->appendChild(Aend2Elem);
                                    AtrackElem->appendChild(AclipitemElem);
                                    audioElem->appendChild(AtrackElem);
                                }   
                            }
                        }
                    }
                }
            }
        }
    }
    mediaElem->appendChild(audioElem); 
    //Duration
    int dur = end;
    ostringstream dur_ss;
    dur_ss << dur;
    durElem->appendChild(_doc->createTextNode(Xml(dur_ss.str().c_str())));
    seqElem->appendChild(nameElem);
    seqElem->appendChild(durElem);
    seqElem->appendChild(rate2Elem);
    seqElem->appendChild(mediaElem);
    
#endif

}

bool FCPFile::getSource(Track* track, PackageSet& packages, SourcePackage** fileSourcePackage,
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






