/*
 * $Id: FCPFile.cpp,v 1.2 2008/10/22 09:32:19 john_f Exp $
 *
 * Final Cut Pro XML file for defining clips, multi-camera clips, etc
 *
 * Copyright (C) 2008  British Broadcasting Corporation
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

#include "FCPFile.h"
#include "CreateAAFException.h"

#include <Logging.h>
#include "AAFFile.h"
#include "XmlTools.h"
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include "Timecode.h"
#include <sstream>
#include "CutsDatabase.h"
#include <Database.h>
#include <MCClipDef.h>
#include <Utilities.h>


using namespace std;
using namespace prodauto;
using namespace xercesc;


FCPFile::FCPFile(string filename)
: _filename(filename), _impl(0), _doc(0), _rootElem(0)
{
    XmlTools::Initialise();
    
    _impl =  DOMImplementationRegistry::getDOMImplementation(X("Core"));
    _doc = _impl->createDocument(
            0,                  // root element namespace URI.
            X("xmeml"),         // root element name
            0);                 // document type object (DTD).

    _rootElem = _doc->getDocumentElement();
    _rootElem->setAttribute(X("version"), X("2"));
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

void FCPFile::addClip(MaterialPackage* materialPackage, PackageSet& packages, int totalClips, std::string fcppath)
{


    static const char * anamorph = "TRUE";
    static const char * ntsc = "FALSE";
    static const char * tb ="25";
    static const char * df ="DF";
    static const char * src = "source";
    int angle = 0;
    std::string temptest = "Main";
    
        angle++;
        std::ostringstream angle_ss;
        angle_ss << angle; 
        //TAGS
        //Rate Tag
        DOMElement * rateElem = _doc->createElement(X("rate"));
        //Timecode Tag
        DOMElement * timecodeElem = _doc->createElement(X("timecode"));		
        //Duration
        DOMElement *durElem1 = _doc->createElement(X("duration"));
        //Duration
    	DOMElement *durElem = _doc->createElement(X("duration"));
	//File Path
	DOMElement * purlElem = _doc->createElement(X("pathurl"));
	//File Tag
	DOMElement * fileElem = _doc->createElement(X("file")); 
	//Media Tag
	DOMElement * mediaElem = _doc->createElement(X("media"));
	//Video Tag
	DOMElement * videoElem = _doc->createElement(X("video"));
	//Audio Tag
	DOMElement * audioElem = _doc->createElement(X("audio"));
	//rate
	DOMElement * ntscElem = _doc->createElement(X("ntsc"));
	ntscElem->appendChild(_doc->createTextNode(X(ntsc)));
	//set rate properties
	DOMElement * tbElem = _doc->createElement(X("timebase"));
	tbElem->appendChild(_doc->createTextNode(X(tb)));
	rateElem->appendChild(ntscElem);
	rateElem->appendChild(tbElem);
	//Open XML Clip
	DOMElement * clipElem = _doc->createElement(X("clip"));
	_rootElem->appendChild(clipElem);

	MaterialPackage * topPackage = materialPackage;

	//printf("Package Name=%s\n",topPackage->name.c_str()); //package name: working


	DOMElement * nameElem = _doc->createElement(X("name"));
	DOMElement * fnameElem = _doc->createElement(X("name"));
	clipElem->setAttribute(X("id"), X(topPackage->name.c_str()));
	nameElem->appendChild(_doc->createTextNode(X(topPackage->name.c_str())));
	DOMElement * anaElem = _doc->createElement(X("anamorphic"));
	anaElem->appendChild(_doc->createTextNode(X(anamorph)));
	//MEDIA INFO
	DOMElement * vsmplchElem = _doc->createElement(X("samplecharacteristics"));
	DOMElement * asmplchElem = _doc->createElement(X("samplecharacteristics"));	

	DOMElement * widthElem = _doc->createElement(X("width"));
	widthElem->appendChild(_doc->createTextNode(X("720")));

	DOMElement * heightElem = _doc->createElement(X("height"));
	heightElem->appendChild(_doc->createTextNode(X("576")));

	DOMElement * smplrteElem = _doc->createElement(X("samplerate"));
	smplrteElem->appendChild(_doc->createTextNode(X("48000")));

	DOMElement * dpthElem = _doc->createElement(X("depth"));
	dpthElem->appendChild(_doc->createTextNode(X("16")));

	DOMElement * layoutElem = _doc->createElement(X("layout"));
	layoutElem->appendChild(_doc->createTextNode(X("stereo")));

	DOMElement * inElem = _doc->createElement(X("in"));
	inElem->appendChild(_doc->createTextNode(X("-1")));

	DOMElement * outElem = _doc->createElement(X("out"));
	outElem->appendChild(_doc->createTextNode(X("-1")));

	DOMElement * ismasterElem = _doc->createElement(X("ismasterclip"));
	ismasterElem->appendChild(_doc->createTextNode(X("TRUE")));

	//Audio Channel Tag- right

	DOMElement * chnlElem = _doc->createElement(X("channelcount"));

	/*switch(topPackage->getType()) //package type test: not working...
	{
	case MATERIAL_PACKAGE :
		printf("Material Package\n");
		break;
	case SOURCE_PACKAGE :
		printf("Source Package\n");
		break;
	default:
		printf("Unknown Package Type\n");
		break;
	}*/
	Rational palEditRate = {25, 1};
	int64_t st_time;

	
	st_time = getStartTime(topPackage, packages, palEditRate); //start frames from midnite

	
	//printf("Package Start Time=%lld\n",st_time);
	Timecode tc(st_time);  //returns true time code
	//printf("Time code=%s\n",tc.Text());

	//Rate details: Timecode, Display Format, Source
	DOMElement * tcElem = _doc->createElement(X("string"));
	tcElem->appendChild(_doc->createTextNode(X(tc.Text())));

	DOMElement * dfElem = _doc->createElement(X("displayformat"));
	dfElem->appendChild(_doc->createTextNode(X(df)));
	
	DOMElement * scElem = _doc->createElement(X("source"));
	scElem->appendChild(_doc->createTextNode(X(src)));

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
	fileElem->setAttribute(X("id"), X(topPackage->name.c_str()));
	clipElem->appendChild(fileElem);

	//timecode inside file
	timecodeElem->appendChild(tcElem);
	timecodeElem->appendChild(dfElem);
	timecodeElem->appendChild(scElem);
	fileElem->appendChild(timecodeElem);	
	int taudio=0;

			

        for (std::vector<prodauto::Track*>::const_iterator iter2 = topPackage->tracks.begin(); iter2 != topPackage->tracks.end(); iter2++) //looks in at Track Detail
	
	{

	    //std::vector<prodauto::Track*>::const_iterator iter2 = topPackage->tracks.begin();
            prodauto::Track * track = *iter2;
	    std::string trackname;
	    std::string name = track->name.c_str();
	    size_t pos = name.find_last_of("/");
	    if(pos != std::string::npos)
	    {
	        trackname = name.substr(pos + 1);
	    }
	    else
	    {
		trackname = name;
	    }
	    //printf("Track Name=%s\n",trackname.c_str()); //track name
            //printf("Source Clip Length=%lld\n",track->sourceClip->length); //track length
	    //printf("Picture Def=%d\n",track->dataDef);


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
		//std::vector myname = package->getUserComments();
		//printf("Comments Name %s\n", myname->name.c_str());
		//printf("Comments Value %s\n", userComments->value);
		FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
		prodauto::Track * track = *iter2;
		std::string filelocation;
		std::string floc = fileDescriptor->fileLocation.c_str();
		size_t pos = floc.find_last_of("/");
		if(pos != std::string::npos)
		{
		    filelocation = floc.substr(pos + 1);
		}
		else
		{
		    filelocation = floc;
		}
                //printf("Location=%s\n",filelocation.c_str()); //file Location
		//printf("File Format=%d\n",fileDescriptor->fileFormat); // file Format

		//File Name and Location
		if (track->dataDef == PICTURE_DATA_DEFINITION)
		{
		    //printf("video\n");
		    //Length
		    std::ostringstream dur_ss;
                    dur_ss << track->sourceClip->length;		
		    durElem->appendChild(_doc->createTextNode(X(dur_ss.str().c_str())));
		    durElem1->appendChild(_doc->createTextNode(X(dur_ss.str().c_str())));
		    purlElem->appendChild(_doc->createTextNode(X(fcppath.c_str())));
		    purlElem->appendChild(_doc->createTextNode(X(filelocation.c_str())));
		    fnameElem->appendChild(_doc->createTextNode(X(trackname.c_str())));
		    //duration into file->media->video
		    videoElem->appendChild(durElem);
		    //width and height into video sample characterstics
		    vsmplchElem->appendChild(widthElem);
		    vsmplchElem->appendChild(heightElem);				
		    //video sample characteristics into file->media->video
		    videoElem->appendChild(vsmplchElem);
	  	    //video into media
		    mediaElem->appendChild(videoElem);
		}
		else if (track->dataDef == SOUND_DATA_DEFINITION)
                {
		     taudio++;
		     if (sourcePackage->sourceConfigName.c_str() == temptest)
		     {
		         if (taudio==1)
		         {
		             //samplerate + depth into sample characteristics
		             asmplchElem->appendChild(smplrteElem);
		             asmplchElem->appendChild(dpthElem);				
		             //audio sample characteristics into file->media->audio
		             audioElem->appendChild(asmplchElem);
		             //layout into audio
		             audioElem->appendChild(layoutElem);
		         }			
		         std::ostringstream t1_ss;
		         t1_ss << taudio;			
		         //audio channel label into audio channel
		         {
		             DOMElement * chnllbrElem = _doc->createElement(X("channellabel"));
		             DOMElement * audiochrElem1 = _doc->createElement(X("audiochannel"));
		             chnllbrElem->appendChild(_doc->createTextNode(X("ch")));
		             chnllbrElem->appendChild(_doc->createTextNode(X(t1_ss.str().c_str())));
		             audiochrElem1->appendChild(chnllbrElem);
		             //audio channel into file->media->audio
		             audioElem->appendChild(audiochrElem1);
		         }
		         mediaElem->appendChild(audioElem) ;
		     }
                 }
		 else
                 {
	             printf("unknown clip type");
                 }
	     }	
	     //printf("---------------------\n");
	     //printf("\n");
	 }    
	 std::ostringstream taudioss;
         taudioss << taudio;	
	 //channel count into audio
	 //printf("ch taudio=%d\n",taudio);
	 chnlElem->appendChild(_doc->createTextNode(X("2")));
	 audioElem->appendChild(chnlElem);	
	 //media into file
	 fileElem->appendChild(mediaElem);
	 //clip details cntd.
	 clipElem->appendChild(inElem);
	 clipElem->appendChild(outElem);
	 //master clip id, name plus M
	 DOMElement * masteridElem = _doc->createElement(X("masterclipid"));
	 masteridElem->appendChild(_doc->createTextNode(X("M")));
	 masteridElem->appendChild(_doc->createTextNode(X(topPackage->name.c_str())));
	 clipElem->appendChild(masteridElem);
	 clipElem->appendChild(ismasterElem);
	 //default angle, must increase by 1 for each clip
	 DOMElement * defangleElem = _doc->createElement(X("defaultangle"));
	 defangleElem->appendChild(_doc->createTextNode(X(angle_ss.str().c_str())));
	 clipElem->appendChild(defangleElem);			
         ++ totalClips;	
   // End of Clip definitions

//return totalClips;
}

void FCPFile::addMCClip(MCClipDef* itmcP, MaterialHolder &material, int idname)
{
    // multi-camera clip and director's cut sequence
    static const char * ntsc = "FALSE";
    static const char * tb ="25";
    int in= -1;
    int out= -1;

    // load multi-camera clip definitions- need MCClip defs    
    Database* database = Database::getInstance();
    VectorGuard<MCClipDef> mcClipDefs;
    mcClipDefs.get() = database->loadAllMultiCameraClipDefs();
    //Media Tag
    DOMElement * mediaElem = _doc->createElement(X("media"));
    vector<MCClipDef*>::iterator itmc;
    DOMElement * _rootElem = _doc->getDocumentElement();
    {

                MaterialPackageSet::const_iterator iter1 = material.topPackages.begin();
		{

	            //De-bugging info
	            //printf("\n");
	            //printf("--------- START OF MULTICLIP ---------\n");

	            std::ostringstream idname_ss;
                    idname_ss << idname;
	            //printf("Multiclip \n");       
  	            int c1 = 0;
	            //DOM Details for Multiclip
	            //TAGS
	            //Rate Tag
	            DOMElement * rateElem = _doc->createElement(X("rate"));

	            DOMElement * rate2Elem = _doc->createElement(X("rate"));

	            //Video Tag
	            DOMElement * videoElem = _doc->createElement(X("video"));
 	            //rate
	            DOMElement * ntscElem = _doc->createElement(X("ntsc"));
	            ntscElem->appendChild(_doc->createTextNode(X(ntsc)));
	            DOMElement * tbElem = _doc->createElement(X("timebase"));
	            tbElem->appendChild(_doc->createTextNode(X(tb)));
	            rateElem->appendChild(ntscElem);
	            rateElem->appendChild(tbElem);

	            DOMElement * ntscElem2 = _doc->createElement(X("ntsc"));
	            ntscElem2->appendChild(_doc->createTextNode(X(ntsc)));
	            DOMElement * tbElem2 = _doc->createElement(X("timebase"));
	            tbElem2->appendChild(_doc->createTextNode(X(tb)));
	            rate2Elem->appendChild(ntscElem2);
	            rate2Elem->appendChild(tbElem2);
	    
                    //Mduration
	            DOMElement * MdurElem = _doc->createElement(X("duration"));
	            DOMElement * Mdur2Elem = _doc->createElement(X("duration"));
	            //Mclipitem1
	            DOMElement * Mclipitem1Elem = _doc->createElement(X("clipitem"));
	            //Multiclip id
	            DOMElement * MclipidElem = _doc->createElement(X("multiclip"));
	            //Mcollapsed
	            DOMElement * McollapsedElem = _doc->createElement(X("collapsed"));
	            //Msynctype
	            DOMElement * MsyncElem = _doc->createElement(X("synctype"));
	            //Mclip
	            DOMElement * MclipElem = _doc->createElement(X("clip"));
	            //Mname
	            DOMElement * MnameElem = _doc->createElement(X("name"));
	            DOMElement * Mname1Elem = _doc->createElement(X("name"));
	            DOMElement * Mname2Elem = _doc->createElement(X("name"));
	            //Track Tag
	            DOMElement * trackElem = _doc->createElement(X("track"));

	            DOMElement * inElem = _doc->createElement(X("in"));
	            std::ostringstream in_ss;
                    in_ss << in;
	            inElem->appendChild(_doc->createTextNode(X(in_ss.str().c_str())));

	            DOMElement * outElem = _doc->createElement(X("out"));
	            std::ostringstream out_ss;
                    out_ss << out;
	            outElem->appendChild(_doc->createTextNode(X(out_ss.str().c_str())));

	            DOMElement * ismasterElem = _doc->createElement(X("ismasterclip"));
	            ismasterElem->appendChild(_doc->createTextNode(X("FALSE")));
		  

	            MaterialPackageSet donePackages;
	            //Name
                    MaterialPackage* topPackage1 = *iter1;
                    MaterialPackageSet materialPackages;
                    materialPackages.insert(topPackage1);

                    //DOM Multicam 
                    // add material to group that has same start time and creation date

                    Rational palEditRate = {25, 1}; 


                    for (MaterialPackageSet::const_iterator iter2 = iter1; iter2 != material.topPackages.end(); iter2++)
                    {
	               MaterialPackage* topPackage2 = *iter2;
		       int64_t st_time;



		       if (topPackage2->creationDate.year == topPackage1->creationDate.year && topPackage2->creationDate.month == topPackage1->creationDate.month && topPackage2->creationDate.day == topPackage1->creationDate.day && getStartTime(topPackage1, material.packages, palEditRate) ==  getStartTime(topPackage2, material.packages, palEditRate))
		       {
			  

			   //looks in at Track Detail
		          materialPackages.insert(topPackage2);
			  donePackages.insert(topPackage2);

			   for (std::vector<prodauto::Track*>::const_iterator iter6 = topPackage2->tracks.begin(); iter6 != topPackage2->tracks.end(); iter6++) 
			   {

			       c1++;
			       prodauto::Track * track = *iter6;
			       if (c1 == 1)
			       {

			           //MClip DOM Start
				   _rootElem->appendChild(MclipElem);

				   MclipElem->setAttribute(X("id"), X(idname_ss.str().c_str()));
				   //Name
				   Mname2Elem->appendChild(_doc->createTextNode(X("Multicam")));
				   Mname2Elem->appendChild(_doc->createTextNode(X(idname_ss.str().c_str())));
				   Mname1Elem->appendChild(_doc->createTextNode(X("Multicam")));
				   Mname1Elem->appendChild(_doc->createTextNode(X(idname_ss.str().c_str())));
				   MnameElem->appendChild(_doc->createTextNode(X("Multicam")));
				   MnameElem->appendChild(_doc->createTextNode(X(idname_ss.str().c_str())));
				   //Duration

				   std::ostringstream tMdurss;
				   tMdurss << track->sourceClip->length;
				   MdurElem->appendChild(_doc->createTextNode(X(tMdurss.str().c_str())));
				   Mdur2Elem->appendChild(_doc->createTextNode(X(tMdurss.str().c_str())));
				   //Clip Items: name
				   Mclipitem1Elem->setAttribute(X("id"), X(idname_ss.str().c_str()));
				   //Collapsed
				   McollapsedElem->appendChild(_doc->createTextNode(X("FALSE")));
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
				   Mclipitem1Elem->setAttribute(X("id"), X(idname_ss.str().c_str()));
				   MclipidElem->appendChild(MnameElem);
				   MclipidElem->appendChild(McollapsedElem);
				   MclipidElem->appendChild(MsyncElem);
				   MclipidElem->setAttribute(X("id"), X(idname_ss.str().c_str()));
	
			        }
			        //Material Package Name
			        SourcePackage dummy;
			        dummy.uid = track->sourceClip->sourcePackageUID;
			        PackageSet::iterator result = material.packages.find(&dummy);
			        vector<MCClipDef*>::iterator itmc1;
				
			        for (itmc1 = mcClipDefs.get().begin(); itmc1 != mcClipDefs.get().end(); itmc1++)
			        {

			            MCClipDef * mcClipDef = itmcP;				
				    if ((*itmc1)->name == itmcP->name)
				    {
				        for (std::vector<SourceConfig*>::const_iterator itmc2 = mcClipDef->sourceConfigs.begin(); itmc2 != mcClipDef->sourceConfigs.end(); itmc2++)
				        {
				            //SourceConfig * sourceConfigs = *itmc2;
				            Package* package = *result;
				            SourcePackage* sourcePackage = dynamic_cast<SourcePackage*>(package);
				            if ((*itmc2)->name == sourcePackage->sourceConfigName)
				            {
				                //printf("\n  <-START OF TRACK DETAILS-> \n");
				    	        //printf("Track file name=%s\n",topPackage2->name.c_str());
				                st_time = getStartTime(topPackage2, material.packages, palEditRate);
				                Timecode tc(st_time);
				                //printf("Time code=%s\n",tc.Text());
				                //printf("Source Config Name %s\n", sourcePackage->sourceConfigName.c_str());
				    	        if (track->dataDef != PICTURE_DATA_DEFINITION) //audio
				                {
					            //printf("Audio Track Name=%s\n",track->name.c_str()); //track name
					            //printf("Audio Source Clip Length=%lld\n",track->sourceClip->length); 
				                }
				                if (track->dataDef == PICTURE_DATA_DEFINITION) //VIDEO
				                { 
					            //Mclip
					            DOMElement * Mclip2Elem = _doc->createElement(X("clip"));
					            //Mclip
					            DOMElement * MfileElem = _doc->createElement(X("file"));
					            //Mangle1
					            DOMElement * Mangle1Elem = _doc->createElement(X("angle"));
					            //printf("Video Track Name=%s\n",track->name.c_str()); //track name
					            //printf("Video Source Clip Length=%lld\n",track->sourceClip->length);
						    Mclip2Elem->setAttribute(X("id"), X(topPackage2->name.c_str()));	
						    MfileElem->setAttribute(X("id"), X(topPackage2->name.c_str()));

						    Mclip2Elem->appendChild(MfileElem);

						    Mangle1Elem->appendChild(Mclip2Elem);

						    MclipidElem->appendChild(Mangle1Elem);

						    Mclipitem1Elem->appendChild(MclipidElem);

						    trackElem->appendChild(Mclipitem1Elem);

						    videoElem->appendChild(trackElem);

						    mediaElem->appendChild(videoElem);

						    MclipElem->appendChild(mediaElem);		
				                }
			    	                //printf("  <-END OF TRACK DETAILS-> \n\n");	
				            }
					}
			            }
			        }
			    }
		        }//if multicam
		    }//for material package
		}
	    }

//audio
	int taudio =0;
   	DOMElement * audioElem = _doc->createElement(X("audio"));
	for (map<uint32_t, MCTrackDef*>::const_iterator itMC2 = itmcP->trackDefs.begin(); itMC2 != itmcP->trackDefs.end(); itMC2++)
	{
	    MCTrackDef* trackDef = (*itMC2).second;
	    for ( std::map<uint32_t, MCSelectorDef*>::const_iterator itMC3 = trackDef->selectorDefs.begin(); itMC3 != trackDef->selectorDefs.end(); itMC3++)
            {
		MCSelectorDef* selectorDef = (*itMC3).second;
		SourceTrackConfig* trackConfig =  selectorDef->sourceConfig->getTrackConfig(selectorDef->sourceTrackID);
 	        if (trackConfig->dataDef == SOUND_DATA_DEFINITION)
	        {
	            printf("*****source track config name %s\n", trackConfig->name.c_str());
                    for (MaterialPackageSet::const_iterator iter2 = material.topPackages.begin(); iter2 != material.topPackages.end(); iter2++)
                    {
	                MaterialPackage* topPackage2 = *iter2;
	                for (std::vector<prodauto::Track*>::const_iterator iter6 = topPackage2->tracks.begin(); iter6 != topPackage2->tracks.end(); iter6 ++)
   	                {
                            prodauto::Track * track = *iter6;
			    SourcePackage* fileSourcePackage;
			    SourcePackage* sourcePackage;
			    Track* sourceTrack;
			    if (! getSource(track, material.packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
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
		                        DOMElement * AformatElem = _doc->createElement(X("format"));
		                        DOMElement * AsmpcharElem = _doc->createElement(X("samplecharacteristics"));
		                        DOMElement * AdepthElem = _doc->createElement(X("depth"));
		                        AdepthElem->appendChild(_doc->createTextNode(X("16")));
		                        DOMElement * AsmprateElem = _doc->createElement(X("samplerate"));
		                        AsmprateElem->appendChild(_doc->createTextNode(X("48000")));
			                AsmpcharElem->appendChild(AdepthElem);
			                AsmpcharElem->appendChild(AsmprateElem);
			                AformatElem->appendChild(AsmpcharElem);
		                        DOMElement * AoutputsElem = _doc->createElement(X("outputs"));
		                        DOMElement * AgroupElem = _doc->createElement(X("group"));
		                        DOMElement * AchannelElem = _doc->createElement(X("channel"));
		                        DOMElement * AindexElem = _doc->createElement(X("index"));
		                        AindexElem->appendChild(_doc->createTextNode(X("1")));
		                        DOMElement * AnumchElem = _doc->createElement(X("numchannels"));
		                        AnumchElem->appendChild(_doc->createTextNode(X("2")));
		                        DOMElement * AdownmixElem = _doc->createElement(X("downmix"));
				        AdownmixElem->appendChild(_doc->createTextNode(X("2")));
				        DOMElement * ACindex1Elem = _doc->createElement(X("index"));
				        DOMElement * ACindex2Elem = _doc->createElement(X("index"));
				        AgroupElem->appendChild(AindexElem);
				        AgroupElem->appendChild(AnumchElem);
				        AgroupElem->appendChild(AdownmixElem);
		                        ACindex1Elem->appendChild(_doc->createTextNode(X("1")));
			                AchannelElem->appendChild(ACindex1Elem);
			                AgroupElem->appendChild(AchannelElem);
		                        ACindex2Elem->appendChild(_doc->createTextNode(X("2")));
			                AchannelElem->appendChild(ACindex2Elem);
			                AgroupElem->appendChild(AchannelElem);
			                AoutputsElem->appendChild(AgroupElem);
			                audioElem->appendChild(AformatElem);
			                audioElem->appendChild(AoutputsElem); 
				    }
		                    printf("file id= %s\n",topPackage2->name.c_str());
		    	            printf("number of tracks =%d\n", taudio);
		    	            printf("Source Package Config name: %s\n",fileSourcePackage->sourceConfigName.c_str()); 
				    DOMElement * Asource2Elem = _doc->createElement(X("sourcetrack"));
				    DOMElement * Amedia2Elem = _doc->createElement(X("mediatype"));
				    Amedia2Elem->appendChild(_doc->createTextNode(X("audio")));
				    std::ostringstream ta_ss;
                    		    ta_ss << taudio;
				    DOMElement * AtrackindexElem = _doc->createElement(X("trackindex"));
				    AtrackindexElem->appendChild(_doc->createTextNode(X(ta_ss.str().c_str())));
		                    DOMElement * AtrackElem = _doc->createElement(X("track"));
				    DOMElement * AclipitemElem = _doc->createElement(X("clipitem"));
				    AclipitemElem->setAttribute(X("id"), X(fileSourcePackage->sourceConfigName.c_str()));
				    DOMElement * AfileElem = _doc->createElement(X("file"));
				    AfileElem->setAttribute(X("id"), X(topPackage2->name.c_str()));
				    DOMElement * Astart2Elem = _doc->createElement(X("start"));
				    Astart2Elem->appendChild(_doc->createTextNode(X("0")));
				    DOMElement * Aend2Elem = _doc->createElement(X("end"));
				    int Length = trackConfig->length / 48000;
				    std::ostringstream le_ss;
                    		    le_ss << Length;
				    Aend2Elem->appendChild(_doc->createTextNode(X(le_ss.str().c_str())));
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
}

// add a multi-camera clip sequence from Directors Cut
void FCPFile::addMCSequence(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,std::vector<CutInfo> sequence)
{
    int end = 0;
    Database* database = Database::getInstance();
    VectorGuard<MCClipDef> mcClipDefs;
    mcClipDefs.get() = database->loadAllMultiCameraClipDefs();

    vector<MCClipDef*>::iterator itMC;
    static const char * ntsc = "FALSE";
    static const char * tb ="25";

//DOM DOC
    DOMElement * _rootElem = _doc->getDocumentElement();

    //TAGS
    //Rate Tag
    DOMElement * rateElem = _doc->createElement(X("rate"));
    //Rate Tag
    DOMElement * rate2Elem = _doc->createElement(X("rate"));
    //Media Tag	
    DOMElement * mediaElem = _doc->createElement(X("media"));
    //Video Tag
    DOMElement * videoElem = _doc->createElement(X("video"));
    //Duration
    DOMElement *durElem = _doc->createElement(X("duration"));
    //rate
    DOMElement * ntscElem = _doc->createElement(X("ntsc"));
    ntscElem->appendChild(_doc->createTextNode(X(ntsc)));
    //set rate properties
    DOMElement * tbElem = _doc->createElement(X("timebase"));
    tbElem->appendChild(_doc->createTextNode(X(tb)));
    rateElem->appendChild(ntscElem);
    rateElem->appendChild(tbElem);
 
    //set rate properties
    DOMElement * tb2Elem = _doc->createElement(X("timebase"));
    tb2Elem->appendChild(_doc->createTextNode(X(tb)));
    rate2Elem->appendChild(tb2Elem);

    //Open XML Clip
    DOMElement * seqElem = _doc->createElement(X("sequence"));
    _rootElem->appendChild(seqElem);
    //Sequence Name
    DOMElement * nameElem = _doc->createElement(X("name"));
    nameElem->appendChild(_doc->createTextNode(X("Directors Cut Sequence")));
    //Sequence ID    
    seqElem->setAttribute(X("id"), X("Directors Cut Sequence"));


    //<format>
    DOMElement * formatElem = _doc->createElement(X("format"));
    //<sample charactistics>
    DOMElement * widthElem = _doc->createElement(X("width"));
    widthElem->appendChild(_doc->createTextNode(X("720")));
    DOMElement * heightElem = _doc->createElement(X("height"));
    heightElem->appendChild(_doc->createTextNode(X("576")));
    DOMElement * pixElem = _doc->createElement(X("pixelaspectratio"));
    pixElem->appendChild(_doc->createTextNode(X("PAL-601")));
    DOMElement * fieldElem = _doc->createElement(X("fielddominance"));
    fieldElem->appendChild(_doc->createTextNode(X("lower")));
    DOMElement * sampleElem = _doc->createElement(X("samplecharacteristics"));
    sampleElem->appendChild(widthElem);
    sampleElem->appendChild(heightElem);
    sampleElem->appendChild(pixElem);
    sampleElem->appendChild(fieldElem);
    sampleElem->appendChild(rateElem);

    //<codec info>
    DOMElement * codecElem = _doc->createElement(X("codec"));
    DOMElement * codecNameElem = _doc->createElement(X("name"));
    codecNameElem->appendChild(_doc->createTextNode(X("Apple DV - PAL")));
    DOMElement * appElem = _doc->createElement(X("appspecificdata"));
    DOMElement * appNameElem = _doc->createElement(X("appname"));
    appNameElem->appendChild(_doc->createTextNode(X("Final Cut Pro")));
    DOMElement * appManElem = _doc->createElement(X("appmanufacturer"));
    appManElem->appendChild(_doc->createTextNode(X("Apple Computer, Inc.")));
    DOMElement * appVSElem = _doc->createElement(X("appversion"));
    appVSElem->appendChild(_doc->createTextNode(X("5.0")));
    DOMElement * dataElem = _doc->createElement(X("data"));
    DOMElement * qtcodecElem = _doc->createElement(X("qtcodec"));
    DOMElement * qtcodecNameElem = _doc->createElement(X("codecname"));
    qtcodecNameElem->appendChild(_doc->createTextNode(X("Apple DV")));
    DOMElement * qtcodectypeElem = _doc->createElement(X("codectypename"));
    qtcodectypeElem->appendChild(_doc->createTextNode(X("DV-PAL")));
    DOMElement * qtcodecspElem = _doc->createElement(X("spatialquality"));
    qtcodecspElem->appendChild(_doc->createTextNode(X("0")));
    DOMElement * qtcodectpElem = _doc->createElement(X("temporalquality"));
    qtcodectpElem->appendChild(_doc->createTextNode(X("0")));
    DOMElement * qtcodeckfElem = _doc->createElement(X("keyframerate"));
    qtcodeckfElem->appendChild(_doc->createTextNode(X("0")));
    DOMElement * qtcodecdataElem = _doc->createElement(X("datarate"));
    qtcodecdataElem->appendChild(_doc->createTextNode(X("0")));
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
    DOMElement * appElem2 = _doc->createElement(X("appspecificdata"));
    DOMElement * appNameElem2 = _doc->createElement(X("appname"));
    appNameElem2->appendChild(_doc->createTextNode(X("Final Cut Pro")));
    DOMElement * appManElem2 = _doc->createElement(X("appmanufacturer"));
    appManElem2->appendChild(_doc->createTextNode(X("Apple Computer, Inc.")));
    DOMElement * appVSElem2 = _doc->createElement(X("appversion"));
    appVSElem2->appendChild(_doc->createTextNode(X("5.0")));
    DOMElement * dataElem2 = _doc->createElement(X("data"));
    DOMElement * fcpimageElem = _doc->createElement(X("fcpimageprocessing"));
    DOMElement * useyuvElem = _doc->createElement(X("useyuv"));
    useyuvElem->appendChild(_doc->createTextNode(X("TRUE")));
    DOMElement * usewhiteElem = _doc->createElement(X("usesuperwhite"));
    usewhiteElem->appendChild(_doc->createTextNode(X("FALSE")));
    DOMElement * renderElem = _doc->createElement(X("rendermode"));
    renderElem->appendChild(_doc->createTextNode(X("YUV8BPP")));
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
    DOMElement * trackElem = _doc->createElement(X("track"));
    int cut = 0;

    int poss = 1;

    for (vector<CutInfo>::const_iterator it = sequence.begin(); it != sequence.end(); ++it)
    {	
        for (MaterialPackageSet::const_iterator iter2 = materialPackages.begin(); iter2 != materialPackages.end(); iter2++)
        {
	    MaterialPackage* topPackage2 = *iter2;
	    std::vector<prodauto::Track*>::const_iterator iter6 = topPackage2->tracks.begin();
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
        	printf("position %lld\n",it->position);
        	printf("video source %s\n",it->source.c_str());	
		printf("Material Package Infomation:\n");
	    	printf("package name: %s\n",topPackage2->name.c_str());
		printf("Source Package Config name: %s\n",sourcePackage->sourceConfigName.c_str());
		printf("--------------------\n");
		printf("\n");
//Video
		//Clip item Tag
	        if (track->dataDef !=  SOUND_DATA_DEFINITION)
	        {
   		    DOMElement * clipitemElem = _doc->createElement(X("clipitem"));
		    clipitemElem->setAttribute(X("id"), X(it->source.c_str()));
		    DOMElement * fileElem = _doc->createElement(X("file"));
   		    DOMElement * clipnameElem = _doc->createElement(X("name"));
		    clipnameElem->appendChild(_doc->createTextNode(X(it->source.c_str())));	
		    fileElem->setAttribute(X("id"), X(topPackage2->name.c_str()));
		    DOMElement * inElem = _doc->createElement(X("in"));
		    DOMElement * outElem = _doc->createElement(X("out"));
		    outElem->appendChild(_doc->createTextNode(X("-1")));
		    DOMElement * startElem = _doc->createElement(X("start"));
	            std::ostringstream start_ss;
                    start_ss << it->position;
		    poss = it->position;
		    inElem->appendChild(_doc->createTextNode(X(start_ss.str().c_str())));
		    startElem->appendChild(_doc->createTextNode(X(start_ss.str().c_str())));
		    DOMElement * endElem = _doc->createElement(X("end"));
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
	            std::ostringstream end_ss;
                    end_ss << end;
		    endElem->appendChild(_doc->createTextNode(X(end_ss.str().c_str())));
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
    videoElem->appendChild(formatElem);
    videoElem->appendChild(trackElem);
    mediaElem->appendChild(videoElem);
//AUDIO
    int taudio =0;
    DOMElement * audioElem = _doc->createElement(X("audio"));
    {
	for (map<uint32_t, MCTrackDef*>::const_iterator itMC2 = mcClipDef->trackDefs.begin(); itMC2 != mcClipDef->trackDefs.end(); itMC2++)
	{
	    MCTrackDef* trackDef = (*itMC2).second;
	    for ( std::map<uint32_t, MCSelectorDef*>::const_iterator itMC3 = trackDef->selectorDefs.begin(); itMC3 != trackDef->selectorDefs.end(); itMC3++)
            {
		MCSelectorDef* selectorDef = (*itMC3).second;
		SourceTrackConfig* trackConfig =  selectorDef->sourceConfig->getTrackConfig(selectorDef->sourceTrackID);
 	        if (trackConfig->dataDef == SOUND_DATA_DEFINITION)
	        {
	            printf("*****source track config name %s\n", trackConfig->name.c_str());
                    for (MaterialPackageSet::const_iterator iter2 = materialPackages.begin(); iter2 != materialPackages.end(); iter2++)
                    {
	                MaterialPackage* topPackage2 = *iter2;
	                for (std::vector<prodauto::Track*>::const_iterator iter6 = topPackage2->tracks.begin(); iter6 != topPackage2->tracks.end(); iter6 ++)
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
		                        DOMElement * AformatElem = _doc->createElement(X("format"));
		                        DOMElement * AsmpcharElem = _doc->createElement(X("samplecharacteristics"));
		                        DOMElement * AdepthElem = _doc->createElement(X("depth"));
		                        AdepthElem->appendChild(_doc->createTextNode(X("16")));
		                        DOMElement * AsmprateElem = _doc->createElement(X("samplerate"));
		                        AsmprateElem->appendChild(_doc->createTextNode(X("48000")));
			                AsmpcharElem->appendChild(AdepthElem);
			                AsmpcharElem->appendChild(AsmprateElem);
			                AformatElem->appendChild(AsmpcharElem);
		                        DOMElement * AoutputsElem = _doc->createElement(X("outputs"));
		                        DOMElement * AgroupElem = _doc->createElement(X("group"));
		                        DOMElement * AchannelElem = _doc->createElement(X("channel"));
		                        DOMElement * AindexElem = _doc->createElement(X("index"));
		                        AindexElem->appendChild(_doc->createTextNode(X("1")));
		                        DOMElement * AnumchElem = _doc->createElement(X("numchannels"));
		                        AnumchElem->appendChild(_doc->createTextNode(X("2")));
		                        DOMElement * AdownmixElem = _doc->createElement(X("downmix"));
				        AdownmixElem->appendChild(_doc->createTextNode(X("2")));
				        DOMElement * ACindex1Elem = _doc->createElement(X("index"));
				        DOMElement * ACindex2Elem = _doc->createElement(X("index"));
				        AgroupElem->appendChild(AindexElem);
				        AgroupElem->appendChild(AnumchElem);
				        AgroupElem->appendChild(AdownmixElem);
		                        ACindex1Elem->appendChild(_doc->createTextNode(X("1")));
			                AchannelElem->appendChild(ACindex1Elem);
			                AgroupElem->appendChild(AchannelElem);
		                        ACindex2Elem->appendChild(_doc->createTextNode(X("2")));
			                AchannelElem->appendChild(ACindex2Elem);
			                AgroupElem->appendChild(AchannelElem);
			                AoutputsElem->appendChild(AgroupElem);
			                audioElem->appendChild(AformatElem);
			                audioElem->appendChild(AoutputsElem);
				    }
		                    printf("file id= %s\n",topPackage2->name.c_str());
		    	            printf("number of tracks =%d\n", taudio);
		    	            printf("Source Package Config name: %s\n",fileSourcePackage->sourceConfigName.c_str()); 
				    DOMElement * Asource2Elem = _doc->createElement(X("sourcetrack"));
				    DOMElement * Amedia2Elem = _doc->createElement(X("mediatype"));
				    Amedia2Elem->appendChild(_doc->createTextNode(X("audio")));
				    std::ostringstream ta_ss;
                    		    ta_ss << taudio;
				    DOMElement * AtrackindexElem = _doc->createElement(X("trackindex"));
				    AtrackindexElem->appendChild(_doc->createTextNode(X(ta_ss.str().c_str())));
		                    DOMElement * AtrackElem = _doc->createElement(X("track"));
				    DOMElement * AclipitemElem = _doc->createElement(X("clipitem"));
				    AclipitemElem->setAttribute(X("id"), X("Audio"));
				    DOMElement * AfileElem = _doc->createElement(X("file"));
				    AfileElem->setAttribute(X("id"), X(topPackage2->name.c_str()));
				    DOMElement * Astart2Elem = _doc->createElement(X("start"));
				    Astart2Elem->appendChild(_doc->createTextNode(X("0")));
				    DOMElement * Aend2Elem = _doc->createElement(X("end"));
				    int Length = end;
				    std::ostringstream le_ss;
                    		    le_ss << Length;
				    Aend2Elem->appendChild(_doc->createTextNode(X(le_ss.str().c_str())));
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
    std::ostringstream dur_ss;
    dur_ss << dur;
    durElem->appendChild(_doc->createTextNode(X(dur_ss.str().c_str())));
    seqElem->appendChild(nameElem);
    seqElem->appendChild(durElem);
    seqElem->appendChild(rate2Elem);
    seqElem->appendChild(mediaElem);
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



