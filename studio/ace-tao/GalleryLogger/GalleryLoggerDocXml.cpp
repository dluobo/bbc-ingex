/*
 * $Id: GalleryLoggerDocXml.cpp,v 1.3 2009/09/18 15:59:05 john_f Exp $
 *
 * Implementation of XML methods in the CGalleryLoggerDoc class.
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


#if 0  // Disabling all XML output

//#include "stdafx.h"
#include <afxwin.h>         // MFC core and standard components

#include "GalleryLoggerDoc.h"

#include "ace/OS_NS_stdlib.h"

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>
//#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>

#if 1 //defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

#include <sstream>

XERCES_CPP_NAMESPACE_USE

#include "XmlTools.h"

static const char * newline = "\r\n";
static const char * newline_indent1 = "\r\n\t";
static const char * newline_indent2 = "\r\n\t\t";

void CGalleryLoggerDoc::ExportSmil(const char * filename) 
{
    // Sort sequences into programme order
    MakeProgrammeOrder();

    XmlTools::Initialise();

    int errorCode = 0;
    DOMDocument * doc;

    // Create the DOM document
    {

       DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(Xml("Core"));

        if (impl != NULL)
        {
            try
            {
                doc = impl->createDocument(
                   //Xml("http://www.w3.org/2001/SMIL20/Language"),    // root element namespace URI.
                   0,
                   Xml("smil"),         // root element name
                   0);                   // document type object (DTD).

                DOMElement* rootElem = doc->getDocumentElement();
                rootElem->setAttribute(Xml("xmlns:smil2"), Xml("http://www.w3.org/2001/SMIL20/Language"));
                rootElem->appendChild(doc->createTextNode(Xml(newline)));

                // Sequences
                for(int s = 0; s < SequenceCount(); ++s)
                {
                    // Use programme order
                    ::Sequence & sequence = * mProgrammeOrder[s]; 

                    for(int t = 0; t < sequence.TakeCount(); ++t)
                    {
                        ::Take & take = sequence.Take(t);
                        if(take.IsGood())
                        {
                            // Recorded Sources
                            // At present we just use the first RecordedSource
                            if(take.SourceCount() > 0)
                            {
                                ::RecordedSource & rec_source = take.RecordedSource(0);

                                // Look for a DV clip
                                for(int c = 0; c < rec_source.ClipCount(); ++c)
                                {
                                    ::Clip & clip = rec_source.Clip(c);

                                    if(Recording::DV == clip.Recording().Format())
                                    {
                                        DOMElement*  seqElem = doc->createElement(Xml("seq"));
                                        rootElem->appendChild(seqElem);
                                        rootElem->appendChild(doc->createTextNode(Xml(newline)));

                                        DOMElement*  videoElem = doc->createElement(Xml("video"));
                                        seqElem->appendChild(doc->createTextNode(Xml(newline)));
                                        seqElem->appendChild(videoElem);
                                        seqElem->appendChild(doc->createTextNode(Xml(newline)));

                                        std::string filename = clip.Recording().FileId();
                                        // remove file://host
                                        if(0 == filename.compare(0, 7, "file://"))
                                        {
                                            size_t pos = filename.find('/', 7);
                                            filename = filename.substr(pos);
                                        }

                                        videoElem->setAttribute(Xml("src"), Xml(filename.c_str()));
                                        Timecode begin = clip.Start();
                                        Timecode end = begin + take.Duration();
                                        char begin_text[10];
                                        char end_text[10];
                                        ACE_OS::itoa(begin.TotalFrames(), begin_text, 10);
                                        ACE_OS::itoa(end.TotalFrames(), end_text, 10);
                                        videoElem->setAttribute(Xml("clipBegin"), Xml(begin_text));
                                        videoElem->setAttribute(Xml("clipEnd"), Xml(end_text));
                                    } //if(Recording::RAWDV == clip.Type())
                                }
                            } //if(take.SourceCount() > 0)
                        } // if take is good
                    }
                }

           }
           catch (const OutOfMemoryException&)
           {
               //XERCES_STD_QUALIFIER cerr << "OutOfMemoryException" << XERCES_STD_QUALIFIER endl;
               errorCode = 5;
           }
           catch (const DOMException& )
           {
               //XERCES_STD_QUALIFIER cerr << "DOMException code is:  " << e.code << XERCES_STD_QUALIFIER endl;
               errorCode = 2;
           }
           catch (...)
           {
               //XERCES_STD_QUALIFIER cerr << "An error occurred creating the document" << XERCES_STD_QUALIFIER endl;
               errorCode = 3;
           }
       }  // (inpl != NULL)
       else
       {
           //XERCES_STD_QUALIFIER cerr << "Requested implementation is not supported" << XERCES_STD_QUALIFIER endl;
           errorCode = 4;
       }
    }

    XmlTools::DomToFile(doc, filename);

    doc->release();

    XmlTools::Terminate();
}


void CGalleryLoggerDoc::ExportXml(const char * filename) 
{
    // Sort sequences into programme order
    MakeProgrammeOrder();

    // Initialize the XML4C2 system.
    XmlTools::Initialise();

    int errorCode = 0;
    DOMDocument * doc;

    // Create the DOM document
   {

        DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(Xml("Core"));

        if (impl != NULL)
        {
            try
            {
                doc = impl->createDocument(
                           0,             // root element namespace URI.
                           Xml("log"),       // root element name
                           0);                 // document type object (DTD).

                DOMElement * rootElem = doc->getDocumentElement();
                rootElem->appendChild(doc->createTextNode(Xml(newline)));

                DOMElement *  seriesElem = doc->createElement(Xml("series"));
                rootElem->appendChild(seriesElem);

                if (this->HasSeries())
                {
                    DOMElement *  tmpElem = doc->createElement(Xml("name"));
                    tmpElem->appendChild(doc->createTextNode(Xml(CurrentSeriesName())));
                    seriesElem->appendChild(tmpElem);
                }

                DOMElement *  programmeElem = doc->createElement(Xml("programme"));
                seriesElem->appendChild(doc->createTextNode(Xml(newline)));
                seriesElem->appendChild(programmeElem);


                if (this->HasProgramme())
                {
                    DOMElement * tmpElem = doc->createElement(Xml("name"));
                    tmpElem->appendChild(doc->createTextNode(Xml(CurrentProgrammeName())));
                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));
                    programmeElem->appendChild(tmpElem);
                }

                if(this->HasProducer())
                {
                    DOMElement * tmpElem = doc->createElement(Xml("producer"));
                    tmpElem->appendChild(doc->createTextNode(Xml(Producer())));
                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));
                    programmeElem->appendChild(tmpElem);
                }

                if(this->HasDirector())
                {
                    DOMElement * tmpElem = doc->createElement(Xml("director"));
                    tmpElem->appendChild(doc->createTextNode(Xml(Director())));
                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));
                    programmeElem->appendChild(tmpElem);
                }

                if(this->HasPA())
                {
                    DOMElement * tmpElem = doc->createElement(Xml("pa"));
                    tmpElem->appendChild(doc->createTextNode(Xml(PA())));
                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));
                    programmeElem->appendChild(tmpElem);
                }

                // Sequences
                for(int s = 0; s < SequenceCount(); ++s)
                {
                    // Use programme order
                    ::Sequence & sequence = * mProgrammeOrder[s]; 
                    //::Sequence & sequence = * mRecordingOrder[s];

                    DOMElement * itemElem = doc->createElement(Xml("item"));
                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));
                    programmeElem->appendChild(itemElem);

                    DOMElement * nameElem = doc->createElement(Xml("name"));
                    itemElem->appendChild(nameElem);
                    nameElem->appendChild(doc->createTextNode(Xml(sequence.Name())));

                    DOMElement * scriptRefElem = doc->createElement(Xml("scriptRef"));
                    itemElem->appendChild(scriptRefElem);
                    scriptRefElem->appendChild(doc->createTextNode(Xml(sequence.ScriptRefRange().c_str())));

                    DOMElement * idElem = doc->createElement(Xml("itemId"));
                    itemElem->appendChild(idElem);
                    idElem->appendChild(doc->createTextNode(Xml(sequence.UniqueId())));

                    DOMElement * durElem = doc->createElement(Xml("targetDuration"));
                    itemElem->appendChild(durElem);
                    durElem->appendChild(doc->createTextNode(Xml(sequence.TargetDuration().TextS())));

                    programmeElem->appendChild(doc->createTextNode(Xml(newline)));

                    // Takes
                    for(int t = 0; t < sequence.TakeCount(); ++t)
                    {
                        ::Take & take = sequence.Take(t);

                        DOMElement *  takeElem = doc->createElement(Xml("take"));
                        itemElem->appendChild(takeElem);

                        char tmp[8];
                        DOMElement *  tmpElem = doc->createElement(Xml("number"));
                        tmpElem->appendChild(doc->createTextNode(
                            Xml(ACE_OS::itoa(take.Number(), tmp, 10))));
                        takeElem->appendChild(tmpElem);

                        DOMElement * dateElem = doc->createElement(Xml("date"));
                        takeElem->appendChild(dateElem);
                        dateElem->appendChild(doc->createTextNode(Xml(take.Date())));

                        DOMElement * inElem = doc->createElement(Xml("in"));
                        takeElem->appendChild(inElem);
                        DOMText * inDataVal = doc->createTextNode(Xml(take.In().Text()));
                        inElem->appendChild(inDataVal);

                        DOMElement * outElem = doc->createElement(Xml("out"));
                        takeElem->appendChild(outElem);
                        outElem->appendChild(doc->createTextNode(Xml(take.Out().Text())));

                        DOMElement * durElem = doc->createElement(Xml("duration"));
                        takeElem->appendChild(durElem);
                        durElem->appendChild(doc->createTextNode(Xml(take.Duration().Text())));

                        DOMElement * resultElem = doc->createElement(Xml("result"));
                        takeElem->appendChild(resultElem);
                        DOMText * resultDataVal = doc->createTextNode(Xml((take.IsGood() ? "Good" : "NG")));
                        resultElem->appendChild(resultDataVal);

                        DOMElement * commentElem = doc->createElement(Xml("comment"));
                        takeElem->appendChild(commentElem);
                        commentElem->appendChild(doc->createTextNode(Xml(take.Comment())));

                        // Recorded Sources
                        for(int r = 0; r < take.SourceCount(); r++)
                        {
                            ::RecordedSource & rec_source = take.RecordedSource(r);

                            DOMElement * sourceElem = doc->createElement(Xml("source"));
                            takeElem->appendChild(sourceElem);

                            DOMElement * elem;
                            elem = doc->createElement(Xml("name"));
                            elem->appendChild(doc->createTextNode(Xml(rec_source.Source().Name())));
                            sourceElem->appendChild(elem);

                            //elem = doc->createElement(Xml("description"));
                            //elem->appendChild(doc->createTextNode(Xml(rec_source.Source().Description())));
                            //sourceElem->appendChild(elem);

                            // Clips
                            for(int c = 0; c < rec_source.ClipCount(); ++c)
                            {
                                ::Clip & clip = rec_source.Clip(c);
                                Recording::EnumeratedFormat fmt = clip.Recording().Format();

                                if(fmt == Recording::FILE) // temporarily only output file info
                                {
                                    DOMElement * clipElem = doc->createElement(Xml("clip"));
                                    sourceElem->appendChild(clipElem);

                                    elem = doc->createElement(Xml("format"));
                                    elem->appendChild(doc->createTextNode(Xml(Recording::FormatText(fmt))));
                                    clipElem->appendChild(elem);

                                    Timecode start;
                                    Timecode end;
                                    switch(fmt)
                                    {
                                    case Recording::TAPE:
                                        elem = doc->createElement(Xml("spool"));
                                        elem->appendChild(doc->createTextNode(Xml(clip.Recording().TapeId())));
                                        clipElem->appendChild(elem);

                                        start = take.In() - Duration("00:00:01:00");
                                        end = take.Out() + Duration("00:00:01:00");
                                        break;

                                    case Recording::DV:
                                    case Recording::SDI:
                                    case Recording::FILE:
                                        elem = doc->createElement(Xml("file"));
                                        elem->appendChild(doc->createTextNode(Xml(clip.Recording().FileId())));
                                        clipElem->appendChild(elem);

                                        start = clip.Recording().FileStartTimecode();
                                        end = clip.Recording().FileEndTimecode();
                                        break;
                                    }
                                    elem = doc->createElement(Xml("startTimecode"));
                                    elem->appendChild(doc->createTextNode(Xml(start.Text())));
                                    clipElem->appendChild(elem);

                                    Duration duration = end - start;
                                    elem = doc->createElement(Xml("duration"));
                                    char tmp[16]; // for itoa conversion
                                    elem->appendChild(doc->createTextNode(Xml(ACE_OS::itoa(duration.TotalFrames(),tmp, 10))));
                                    clipElem->appendChild(elem);
                                }
                                sourceElem->appendChild(doc->createTextNode(Xml(newline)));
                            } // for Clip
                        } // for RecordedSource

                        itemElem->appendChild(doc->createTextNode(Xml(newline)));
                    } // for take
                } // for programme

           }
           catch (const OutOfMemoryException&)
           {
               //XERCES_STD_QUALIFIER cerr << "OutOfMemoryException" << XERCES_STD_QUALIFIER endl;
               errorCode = 5;
           }
           catch (const DOMException& )
           {
               //XERCES_STD_QUALIFIER cerr << "DOMException code is:  " << e.code << XERCES_STD_QUALIFIER endl;
               errorCode = 2;
           }
           catch (...)
           {
               //XERCES_STD_QUALIFIER cerr << "An error occurred creating the document" << XERCES_STD_QUALIFIER endl;
               errorCode = 3;
           }
       }  // (inpl != NULL)
       else
       {
           //XERCES_STD_QUALIFIER cerr << "Requested implementation is not supported" << XERCES_STD_QUALIFIER endl;
           errorCode = 4;
       }
   }

    XmlTools::DomToFile(doc, filename);

    doc->release();

    XmlTools::Terminate();
}

void CGalleryLoggerDoc::ExportToFcp(const char * filename)
{
    // Sort sequences into programme order
    MakeProgrammeOrder();

    // Initialize the XML4C2 system.
    XmlTools::Initialise();

    // Create the DOM document
    DOMImplementation * impl =  DOMImplementationRegistry::getDOMImplementation(Xml("Core"));
    DOMDocument * doc;

    if(impl != 0)
    {
        try
        {
            DOMDocumentType * docType = impl->createDocumentType(Xml("xmeml"), 0, 0);
            doc = impl->createDocument(
                       0,             // root element namespace URI.
                       Xml("xmeml"),       // root element name
                       docType);                 // document type object (DTD).
            DOMElement * rootElem = doc->getDocumentElement();
            rootElem->setAttribute(Xml("version"),Xml("1"));
            rootElem->appendChild(doc->createTextNode(Xml(newline)));

            // bin
            DOMElement * binElem = doc->createElement(Xml("bin"));
            rootElem->appendChild(binElem);

            // bin-name
            DOMElement * nameElem = doc->createElement(Xml("name"));
            nameElem->appendChild(doc->createTextNode(Xml(this->CurrentProgrammeName())));
            binElem->appendChild(nameElem);

            // children
            DOMElement * childrenElem = doc->createElement(Xml("children"));
            binElem->appendChild(childrenElem);

            // Items
            for(int s = 0; s < SequenceCount(); ++s)
            {
                ::Sequence & item = * mProgrammeOrder[s];

                // Takes
                for(int t = 0; t < item.TakeCount(); ++t)
                {
                    ::Take & take = item.Take(t);

                    if(take.IsGood())
                    {
                        // Recorded Sources
                        for(int rs = 0; rs < take.SourceCount(); ++rs)
                        {
                            ::RecordedSource & rec_source = take.RecordedSource(rs);

                            // Pick out the required source
                            //if(0 == ACE_OS::strcmp(rec_source.Source().Name(),
                            //  mSources[src].Name()))
                            {

                                // clips
                                for(int c = 0; c < rec_source.ClipCount(); ++c)
                                {
                                    ::Clip & clip = rec_source.Clip(c);

                                    if(Recording::SDI == clip.Recording().Format())
                                    {
                                        // generate clip name/id
                                        std::ostringstream name_ss;
                                        name_ss << item.UniqueId()
                                            << "_T" << take.Number()
                                            << "_" << rec_source.Source().Name();

                                        // clip
                                        DOMElement * clipElem = doc->createElement(Xml("clip"));
                                        //clipElem->setAttribute(Xml("id"),Xml(name_ss.str().c_str()));
                                        childrenElem->appendChild(clipElem);

                                        // name
                                        DOMElement * nameElem = doc->createElement(Xml("name"));
                                        nameElem->appendChild(doc->createTextNode(Xml(name_ss.str().c_str())));
                                        clipElem->appendChild(nameElem);

                                        // logginginfo
                                        DOMElement * logginginfoElem = doc->createElement(Xml("logginginfo"));
                                        clipElem->appendChild(nameElem);

                                        // logginginfo-good
                                        DOMElement * goodElem = doc->createElement(Xml("good"));
                                        goodElem->appendChild(doc->createTextNode(Xml(
                                            (take.IsGood() ? "true" : "false"))));
                                        logginginfoElem->appendChild(goodElem);

                                        // ismasterclip
                                        DOMElement * ismasterclipElem = doc->createElement(Xml("ismasterclip"));
                                        ismasterclipElem->appendChild(doc->createTextNode(Xml("true")));
                                        clipElem->appendChild(ismasterclipElem);

                                        // calculate duration
                                        Timecode start = clip.Recording().FileStartTimecode();
                                        Timecode end = clip.Recording().FileEndTimecode();
                                        Duration duration = end - start;
                                        std::ostringstream dur_ss;
                                        dur_ss << duration.TotalFrames();

                                        // duration
                                        DOMElement * durationElem = doc->createElement(Xml("duration"));
                                        durationElem->appendChild(doc->createTextNode(Xml(dur_ss.str().c_str())));
                                        clipElem->appendChild(durationElem);

                                        // rate
                                        DOMElement * rateElem = doc->createElement(Xml("rate"));
                                        clipElem->appendChild(rateElem);

                                        // ntsc
                                        DOMElement * ntscElem = doc->createElement(Xml("ntsc"));
                                        ntscElem->appendChild(doc->createTextNode(Xml("false")));
                                        rateElem->appendChild(ntscElem);

                                        // timebase
                                        DOMElement * timebaseElem = doc->createElement(Xml("timebase"));
                                        timebaseElem->appendChild(doc->createTextNode(Xml("25")));
                                        rateElem->appendChild(timebaseElem);

#if 1
                                        // file
                                        DOMElement * fileElem = doc->createElement(Xml("file"));
                                        clipElem->appendChild(fileElem);

                                        // name
                                        nameElem = doc->createElement(Xml("name"));
                                        nameElem->appendChild(doc->createTextNode(Xml(name_ss.str().c_str())));
                                        fileElem->appendChild(nameElem);

                                        // pathurl
                                        std::ostringstream pathurl_ss;
                                        pathurl_ss << "file://localhost/dplacie"
                                            //<< this->Programme()
                                            << "/DV/rawdv/"
                                            << this->CurrentProgrammeName()
                                            << "_S"
                                            << name_ss.str()
                                            << "_v1.dv";
                                        DOMElement * pathurlElem = doc->createElement(Xml("pathurl"));
                                        pathurlElem->appendChild(doc->createTextNode(Xml(pathurl_ss.str().c_str())));
                                        fileElem->appendChild(pathurlElem);
#endif

                                        // timecode
                                        DOMElement * timecodeElem = doc->createElement(Xml("timecode"));
                                        //fileElem->appendChild(timecodeElem);
                                        clipElem->appendChild(timecodeElem);

                                        // timecode string
                                        DOMElement * stringElem = doc->createElement(Xml("string"));
                                        timecodeElem->appendChild(stringElem);
                                        stringElem->appendChild(doc->createTextNode(Xml(start.Text())));

                                        // According to spec we only need string
                                        // timecode frame
                                        //std::ostringstream frame_ss;
                                        //frame_ss << start.TotalFrames();
                                        //DOMElement * frameElem = doc->createElement(Xml("frame"));
                                        //timecodeElem->appendChild(frameElem);
                                        //frameElem->appendChild(doc->createTextNode(Xml(frame_ss.str().c_str())));

                                        // timecode displayformat
                                        //DOMElement * displayformatElem = doc->createElement(Xml("displayformat"));
                                        //timecodeElem->appendChild(displayformatElem);
                                        //displayformatElem->appendChild(doc->createTextNode(Xml("NDF")));

                                        // media
                                        DOMElement * mediaElem = doc->createElement(Xml("media"));
                                        clipElem->appendChild(mediaElem);

                                        // video
                                        DOMElement * videoElem = doc->createElement(Xml("video"));
                                        mediaElem->appendChild(videoElem);

                                        // track
                                        DOMElement * trackElem = doc->createElement(Xml("track"));
                                        videoElem->appendChild(trackElem);

                                    }
                                } // for clip
                            } // source selection
                        } // for recorded source
                    } // take selection
                } // for take
            } // for item

            // Finally we write out the file
            XmlTools::DomToFile(doc, filename);

        } // try
        catch (const OutOfMemoryException &)
        {
        }
        catch (const DOMException & )
        {
        }
        catch (...)
        {
        }
    }

    doc->release();
    XmlTools::Terminate();
}

void CGalleryLoggerDoc::ExportToFcpOld(const char * filename) 
{
    // Sort sequences into programme order
    MakeProgrammeOrder();


    XmlTools::Initialise();

    DOMBuilder * parser = 0;
    DOMDocument * doc = 0;
    XmlTools::FileToDom("C:\\XML\\FinalCutPro\\sequence_skeleton.xml", parser, doc);

    Duration timeline_pos;  // will be intialised to zero
    Duration script_timeline_pos;
    int clip_index = 1;

    // Sequences
    for(int s = 0; s < SequenceCount(); ++s)
    {
        // Use programme order
        ::Sequence & sequence = * mProgrammeOrder[s];

#if 1
// Need a separate script description to do this properly
        // Put down a video track of still image captions,
        // to show intended timing according to script.
        // Sequences that are not part of script, e.g. CU re-run
        // should have zero target duration.
        if(sequence.TargetDuration().TotalFrames() > 0)
        {
            std::string name = sequence.Name();
            int track = 0;
            int start = script_timeline_pos.TotalFrames();
            int end = start + sequence.TargetDuration().TotalFrames();
            std::stringstream jpeg_file;
            jpeg_file << "BAMZOOKi_Prog03_" << sequence.Start().Value() << ".jpg"; // fix
            AddFcpStill(doc, name, track, start, end, jpeg_file.str());
            script_timeline_pos += sequence.TargetDuration();
        }
#endif
#if 0
        // Put good takes on timeline.
        for(int t = 0; t < sequence.TakeCount(); ++t)
        {
            ::Take & take = sequence.Take(t);
            if(take.IsGood())
            {
                // Recorded Sources
                for(int rs = 0; rs < take.SourceCount(); ++rs)
                {
                    ::RecordedSource & rec_source = take.RecordedSource(rs);

                    // Look for a DV clip
                    for(int c = 0; c < rec_source.ClipCount(); ++c)
                    {
                        ::Clip & clip = rec_source.Clip(c);

                        if(Recording::DV == clip.Recording().Format())
                        {
                            Timecode clip_in = clip.Start();
                            Timecode clip_out = clip_in + take.Duration();

                            std::stringstream id;
                            id << sequence.UniqueId() << "t" << take.Number()
                                << "-" << rs;
                            int track = rs + 1;
                            AddFcpClip(doc,
                                clip_index,
                                sequence.Name(), id.str(),
                                track,
                                timeline_pos.TotalFrames(),
                                clip_in.TotalFrames(),
                                clip_out.TotalFrames(),
                                clip.Recording().FileId(),
                                0);  // total duration of recording not known

                        } //if(Recording::DV == clip.Type())
                    }
                } //for RecordedSources
                ++clip_index;
                timeline_pos += take.Duration();
            } // if take is good
        }
#endif
    }


    XmlTools::DomToFile(doc, filename);

    parser->release(); // this will also release doc

    XmlTools::Terminate();
}

/**
@param doc DOM representation of XML we are builind up
@param trackNo video track number (starting from 0)
@param start start position on timeline
@param in in-point within clip
@param out out-point within clip
@param durationInFrames duration of whole clip
*/
void CGalleryLoggerDoc::AddFcpClip(DOMDocument * doc,
                                   int clipIndex,
                                   std::string name, std::string id,
                                   int trackNo, int start, int in, int out,
                                   std::string pathurl,
                                   int durationInFrames)
{
    if(durationInFrames < out)
    {
        durationInFrames = out;
    }
    int end = (out - in) + start;

    // Get filename
    std::string filename;
    size_t pos = pathurl.find_last_of('/');
    if(pos != std::string::npos)
    {
        filename = pathurl.substr(pos + 1);
    }
    else
    {
        filename = pathurl;
    }

    // Modify the pathurl for drive mounted on mac
    std::string modified_pathurl = "file://localhost/Volumes/BBCRD%3BPCX208";
    pos = pathurl.find("/prodAuto");
    if(pos != std::string::npos)
    {
        modified_pathurl += pathurl.substr(pos);
        pathurl = modified_pathurl;
    }


    char tmp[16];  // for use by itoa

    int v_track = trackNo + 1;
    int a1_track = trackNo * 2 + 1;
    int a2_track = trackNo * 2 + 2;

    DOMElement * sequence = static_cast<DOMElement *> (doc->getElementsByTagName(Xml("sequence"))->item(0));
    DOMElement * media = static_cast<DOMElement *> (sequence->getElementsByTagName(Xml("media"))->item(0));
    DOMElement * video = static_cast<DOMElement *> (media->getElementsByTagName(Xml("video"))->item(0));

    while (video->getElementsByTagName(Xml("track"))->getLength() < (trackNo + 1))
    {
        video->appendChild(doc->createElement(Xml("track")));
    }

    DOMNode * trackTag = video->getElementsByTagName(Xml("track"))->item(trackNo);
    // We now have the relevant video track

    // Add a video clipitem
    DOMElement * clipitemTag = doc->createElement(Xml("clipitem"));
    clipitemTag->setAttribute(Xml("id"),Xml(id.c_str()));  // set id for clipitem
    trackTag->appendChild(doc->createTextNode(Xml(newline)));
    trackTag->appendChild(clipitemTag);

    DOMElement * nameTag = doc->createElement(Xml("name"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(nameTag);
    DOMText * nameText = doc->createTextNode(Xml(name.c_str()));
    nameTag->appendChild(nameText);

    DOMElement * durationTag = doc->createElement(Xml("duration"));
    clipitemTag->appendChild(durationTag);
    std::stringstream durStr;
    durStr << durationInFrames;
    durationTag->appendChild(doc->createTextNode(Xml(durStr.str().c_str())));

    DOMElement * startTag = doc->createElement(Xml("start"));
    clipitemTag->appendChild(startTag);
    std::stringstream startStr;
    startStr << start;
    DOMText * startText = doc->createTextNode(Xml(startStr.str().c_str()));
    startTag->appendChild(startText);

    DOMElement * endTag = doc->createElement(Xml("end"));
    clipitemTag->appendChild(endTag);
    std::stringstream endStr;
    endStr << end;
    DOMText * endText = doc->createTextNode(Xml(endStr.str().c_str()));
    endTag->appendChild(endText);

    DOMElement * inTag = doc->createElement(Xml("in"));
    clipitemTag->appendChild(inTag);
    std::stringstream inStr;
    inStr << in;
    DOMText * inText = doc->createTextNode(Xml(inStr.str().c_str()));
    inTag->appendChild(inText);

    DOMElement * outTag = doc->createElement(Xml("out"));
    clipitemTag->appendChild(outTag);
    std::stringstream outStr;
    outStr << out;
    DOMText * outText = doc->createTextNode(Xml(outStr.str().c_str()));
    outTag->appendChild(outText);

    // The file element
    DOMElement * fileTag = doc->createElement(Xml("file"));
    fileTag->setAttribute(Xml("id"),Xml(filename.c_str()));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(fileTag);

    DOMElement * pathurlTag = doc->createElement(Xml("pathurl"));
    fileTag->appendChild(pathurlTag);
    DOMText * urlText = doc->createTextNode(Xml(pathurl.c_str()));
    pathurlTag->appendChild(urlText);

    // Link video and audio tracks
    DOMElement * linkElem = doc->createElement(Xml("link"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(linkElem);
    DOMElement * elem = doc->createElement(Xml("mediatype"));
    elem->appendChild(doc->createTextNode(Xml("video")));
    linkElem->appendChild(elem);
    elem = doc->createElement(Xml("trackindex"));
    elem->appendChild(doc->createTextNode(Xml(ACE_OS::itoa(v_track,tmp,10))));
    linkElem->appendChild(elem);
    std::stringstream index;
    index << clipIndex;
    elem = doc->createElement(Xml("clipindex"));
    elem->appendChild(doc->createTextNode(Xml(index.str().c_str())));
    linkElem->appendChild(elem);

    linkElem = doc->createElement(Xml("link"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(linkElem);
    elem = doc->createElement(Xml("mediatype"));
    elem->appendChild(doc->createTextNode(Xml("audio")));
    linkElem->appendChild(elem);
    elem = doc->createElement(Xml("trackindex"));
    elem->appendChild(doc->createTextNode(Xml(ACE_OS::itoa(a1_track,tmp,10))));
    linkElem->appendChild(elem);
    elem = doc->createElement(Xml("clipindex"));
    elem->appendChild(doc->createTextNode(Xml(index.str().c_str())));
    linkElem->appendChild(elem);

    linkElem = doc->createElement(Xml("link"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(linkElem);
    elem = doc->createElement(Xml("mediatype"));
    elem->appendChild(doc->createTextNode(Xml("audio")));
    linkElem->appendChild(elem);
    elem = doc->createElement(Xml("trackindex"));
    elem->appendChild(doc->createTextNode(Xml(ACE_OS::itoa(a2_track,tmp,10))));
    linkElem->appendChild(elem);
    elem = doc->createElement(Xml("clipindex"));
    elem->appendChild(doc->createTextNode(Xml(index.str().c_str())));
    linkElem->appendChild(elem);

    // Add audio clipitems (one for each channel)
    DOMElement * audio = static_cast<DOMElement *> (media->getElementsByTagName(Xml("audio"))->item(0));

    while (audio->getElementsByTagName(Xml("track"))->getLength() < (2 * (trackNo + 1)))
    {
        audio->appendChild(doc->createElement(Xml("track")));
    }

    for(int channel = 0; channel < 2; ++channel)
    {
        // Get track
        trackTag = audio->getElementsByTagName(Xml("track"))->item(channel + 2 * trackNo);

        // Add audio clipitem
        clipitemTag = doc->createElement(Xml("clipitem"));
        clipitemTag->setAttribute(Xml("id"),Xml(id.c_str()));  // set id for clipitem
        trackTag->appendChild(doc->createTextNode(Xml(newline)));
        trackTag->appendChild(clipitemTag);
        // Note that we have used same id as video clipitem so we don't need
        // to repeat the in/out/start/end data.

        // Add file element
        DOMElement * fileTag = doc->createElement(Xml("file"));
        fileTag->setAttribute(Xml("id"),Xml(filename.c_str()));
        clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
        clipitemTag->appendChild(fileTag);
        // Note that we have used same id as video file so we don't need
        // to repeat the pathurl etc.
    }
}

void CGalleryLoggerDoc::AddFcpStill(DOMDocument * doc,
                                    std::string name,
                                    int track,
                                    int start,
                                    int end,
                                    std::string jpeg_file)
{
    // Set pathurl for drive mounted on mac
    //std::string pathurl = "file://localhost/Volumes/BBCRD%3BPCX208/prodAuto/jpegs/";
    std::string pathurl = "file://localhost/Volumes/video/John/jpegs/";
    pathurl += jpeg_file;

    // Calc a duration as this is a required element for clipitem
    // although somewhat meaningless for a still frame.
    int duration = end - start;

    char tmp[16];  // for use by itoa

    DOMElement * sequence = static_cast<DOMElement *> (doc->getElementsByTagName(Xml("sequence"))->item(0));
    DOMElement * media = static_cast<DOMElement *> (sequence->getElementsByTagName(Xml("media"))->item(0));
    DOMElement * video = static_cast<DOMElement *> (media->getElementsByTagName(Xml("video"))->item(0));

    while (video->getElementsByTagName(Xml("track"))->getLength() < (track + 1))
    {
        video->appendChild(doc->createElement(Xml("track")));
    }

    DOMNode * trackTag = video->getElementsByTagName(Xml("track"))->item(track);
    // We now have the relevant video track

    // Add a video clipitem
    DOMElement * clipitemTag = doc->createElement(Xml("clipitem"));
    //clipitemTag->setAttribute(Xml("id"),Xml(id.c_str()));  // set id for clipitem
    trackTag->appendChild(doc->createTextNode(Xml(newline)));
    trackTag->appendChild(clipitemTag);

    DOMElement * nameTag = doc->createElement(Xml("name"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(nameTag);
    DOMText * nameText = doc->createTextNode(Xml(name.c_str()));
    nameTag->appendChild(nameText);

    DOMElement * durationTag = doc->createElement(Xml("duration"));
    clipitemTag->appendChild(durationTag);
    durationTag->appendChild(doc->createTextNode(Xml(ACE_OS::itoa(duration,tmp,10))));

    DOMElement * startTag = doc->createElement(Xml("start"));
    clipitemTag->appendChild(startTag);
    DOMText * startText = doc->createTextNode(Xml(ACE_OS::itoa(start,tmp,10)));
    startTag->appendChild(startText);

    DOMElement * endTag = doc->createElement(Xml("end"));
    clipitemTag->appendChild(endTag);
    DOMText * endText = doc->createTextNode(Xml(ACE_OS::itoa(end,tmp,10)));
    endTag->appendChild(endText);

    DOMElement * stillframeElem = doc->createElement(Xml("stillframe"));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(stillframeElem);
    stillframeElem->appendChild(doc->createTextNode(Xml("TRUE")));

    // The file element
    DOMElement * fileTag = doc->createElement(Xml("file"));
    //fileTag->setAttribute(Xml("id"),Xml(filename.c_str()));
    clipitemTag->appendChild(doc->createTextNode(Xml(newline)));
    clipitemTag->appendChild(fileTag);

    DOMElement * pathurlTag = doc->createElement(Xml("pathurl"));
    fileTag->appendChild(pathurlTag);
    DOMText * urlText = doc->createTextNode(Xml(pathurl.c_str()));
    pathurlTag->appendChild(urlText);

}

#endif // Disabling all XML output


