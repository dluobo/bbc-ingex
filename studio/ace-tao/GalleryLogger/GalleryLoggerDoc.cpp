/*
 * $Id: GalleryLoggerDoc.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Implementation of the "document" class of document/view
 * in Gallery Logger application.
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

#include "stdafx.h"
#include "GalleryLogger.h"

#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_stdlib.h>
#include <ace/OS_NS_string.h>
#include <ace/OS_NS_sys_stat.h>
#include <ace/Log_Msg.h>

#include <fstream>
#include <algorithm> // for sort

#include <database/src/DBException.h>
#include <database/src/DatabaseEnums.h>

#include "GalleryLoggerDoc.h"
#include "SwdFile.h"

#if 0
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerDoc

IMPLEMENT_DYNCREATE(CGalleryLoggerDoc, CDocument)

BEGIN_MESSAGE_MAP(CGalleryLoggerDoc, CDocument)
    //{{AFX_MSG_MAP(CGalleryLoggerDoc)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerDoc construction/destruction

CGalleryLoggerDoc::CGalleryLoggerDoc()
: mSeriesIndex(-1), mProgrammeIndex(-1), mItemIndex(-1), mIsEmpty(true), mBackupIndex(0)
{
    // TODO: add one-time construction code here

    // Init database
    try
    {
        prodauto::Database::initialise("prodautodb","bamzooki","bamzooki",5,10);
        mpDb = prodauto::Database::getInstance();
    }
    catch(...)
    {
        mpDb = 0;
    }
}

CGalleryLoggerDoc::~CGalleryLoggerDoc()
{
    // Close database
    try
    {
        prodauto::Database::close();
    }
    catch(...)
    {
    }
}

BOOL CGalleryLoggerDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    // TODO: add reinitialization code here
    // (SDI documents will reuse this document)

    if (mpDb)
    {
        try
        {
            mSeriesVector = mpDb->loadAllSeries();
        }
        catch(...)
        {
        }
    }

    // tmp hard-coded parameters for testing
    //SelectSeries("BAMZOOKi", false); 
    //SelectProgramme("Grand Final", false);

    return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerDoc serialization

const char * source_id = "Source";
const char * series_id = "Series";
const char * prog_id = "Programme";
const char * seq_id = "Sequence";
const char * take_id = "Take";
const char * recorded_source_id = "RecordedSource";
const char * clip_id = "Clip";
const char * delim = "|";
const char * newline = "\r\n";
const char * blank = "x";
const char * empty = "";
const char * good = "Good";
const char * not_good = "NG";
const char * yes = "yes";
const char * no = "no";
const char * type_tape = "TAPE";
const char * type_file = "FILE";
const char * format_tape = "TAPE";
const char * format_file = "FILE";
const char * format_dv = "DV";
const char * format_sdi = "UNCOMPRESSED";

void CGalleryLoggerDoc::Serialize(CArchive & ar)
{
#if 0 // Not doing loading/saving now we are using database
    if (ar.IsStoring())
    {
    // Storing
#if 0
        // Sources
        for(int i=0; i<mSources.size(); ++i)
        {
            ar.WriteString(source_id);

            ArWrite(ar, mSources[i].Name());

            ArWrite(ar, mSources[i].Description());

            ArWrite(ar, mSources[i].TapeRecorder().Identifier());

            ArWrite(ar, (mSources[i].Enabled() ? yes : no));

            ar.WriteString(newline);
        }
#endif
        // Series
        ar.WriteString(series_id);
        ar.WriteString(delim);
        if(mSeries.size() > 0)
        {
            ar.WriteString(mSeries.c_str());
        }
        else
        {
            ar.WriteString(blank);
        }
        ar.WriteString(newline);

        // Programme
        ar.WriteString(prog_id);
        ar.WriteString(delim);
        // Programme Name
        if(mProgramme.size() > 0)
        {
            ar.WriteString(mProgramme.c_str());
        }
        else
        {
            ar.WriteString(blank);
        }
        ar.WriteString(delim);
        // PA
        if(mPA.size() > 0)
        {
            ar.WriteString(mPA.c_str());
        }
        else
        {
            ar.WriteString(blank);
        }
        ar.WriteString(delim);
        // Director
        if(mDirector.size() > 0)
        {
            ar.WriteString(mDirector.c_str());
        }
        else
        {
            ar.WriteString(blank);
        }
        ar.WriteString(delim);
        // Producer
        if(mProducer.size() > 0)
        {
            ar.WriteString(mProducer.c_str());
        }
        else
        {
            ar.WriteString(blank);
        }
        ar.WriteString(newline);

        // Sequences
        for(unsigned int s = 0; s < mRecordingOrder.size(); ++s)
        {
            ::Sequence & sequence = * mRecordingOrder[s];

            ar.WriteString(seq_id);

            //ar.WriteString(delim);
            //ar.WriteString(sequence.Name());
            ArWrite(ar, sequence.Name());
            //ar.WriteString(delim);
            //ar.WriteString(sequence.Start().Value());
            ArWrite(ar, sequence.Start().Value());
            //ar.WriteString(delim);
            //ar.WriteString(sequence.End().Value());
            //ArWrite(ar, sequence.End().Value());
            ArWrite(ar, blank); // no longer using sequence end
            //ar.WriteString(delim);
            //ar.WriteString(sequence.UniqueId());
            ArWrite(ar, sequence.UniqueId());
            ar.WriteString(newline);

            // Takes
            for(int t = 0; t < sequence.TakeCount(); ++t)
            {
                ::Take & take = sequence.Take(t);

                ar.WriteString(take_id);
                ar.WriteString(delim);
                char tmp[8];
                ar.WriteString(ACE_OS::itoa(take.Number(), tmp, 10));
                ar.WriteString(delim);
                ar.WriteString(take.DateText().c_str());
                ar.WriteString(delim);
                ar.WriteString(take.In().Text());
                ar.WriteString(delim);
                ar.WriteString(take.Out().Text());

                ArWrite(ar, take.Comment());

                ar.WriteString(delim);
                ar.WriteString((take.IsGood() ? good : not_good));
                ar.WriteString(newline);

#if 0
                // Recorded Sources
                for(int r = 0; r < take.SourceCount(); r++)
                {
                    ::RecordedSource & rec_source = take.RecordedSource(r);

                    ar.WriteString(recorded_source_id);

                    ArWrite(ar, rec_source.Source().Name());
                    ArWrite(ar, rec_source.Source().Description());

                    ar.WriteString(newline);

                    // Clips
                    for(int c = 0; c < rec_source.ClipCount(); ++c)
                    {
                        ::Clip & clip = rec_source.Clip(c);

                        ar.WriteString(clip_id);

                        switch(clip.Recording().Format())
                        {
                        case Recording::TAPE:
                            ArWrite(ar, type_tape);
                            break;
                        case Recording::DV:
                        case Recording::SDI:
                        case Recording::FILE:
                            ArWrite(ar, type_file);
                            break;
                        }
                        // Could write parameters according to type
                        // but easier to write all.
                        ArWrite(ar, clip.Recording().TapeId());
                        ArWrite(ar, clip.Recording().FileId());
                        ArWrite(ar, clip.Recording().FileStartTimecode().Text());
                        ArWrite(ar, clip.Recording().FileEndTimecode().Text());

                        switch(clip.Recording().Format())
                        {
                        case Recording::TAPE:
                            ArWrite(ar, format_tape);
                            break;
                        case Recording::FILE:
                            ArWrite(ar, format_file);
                            break;
                        case Recording::DV:
                            ArWrite(ar, format_dv);
                            break;
                        case Recording::SDI:
                            ArWrite(ar, format_sdi);
                            break;
                        }

                        ar.WriteString(newline);
                    }
                }
#endif
            }
        }
    }
    else
    {
    // Loading
        const int bufsize = 1024;
        char buf[bufsize];
        while(ar.ReadString(buf, bufsize - 1) != NULL)
        {
#if 0
            if(0 == strncmp(buf, source_id, strlen(source_id)))
            {
            // Source (View information)
                strtok(buf, delim);
                char * name = strtok(NULL, delim);
                char * description = strtok(NULL, delim);
                char * tape = strtok(NULL, delim);
                char * enabled = strtok(NULL, delim);
                if(NULL != enabled)
                {
                    Source source;
                    // Source Name
                    if(0 != strcmp(name, blank))
                    {
                        source.Name(name);
                    }
                    // Source Description
                    if(0 != strcmp(description, blank))
                    {
                        source.Description(description);
                    }
                    // Tape Number
                    if(0 != strcmp(tape, blank))
                    {
                        source.TapeRecorder().Identifier(tape);
                    }
                    // Enabled ?
                    if(0 == strcmp(enabled, yes))
                    {
                        source.Enabled(true);
                    }
                    else
                    {
                        source.Enabled(false);
                    }
                    mSources.push_back(source);
                }
            }
            else
#endif
            if(0 == strncmp(buf, series_id, strlen(series_id)))
            {
            // Series
                strtok(buf, delim);
                char * name = strtok(NULL, delim);
                if(NULL != name)
                {
                    // Series Name
                    if(0 == strcmp(name, blank))
                    {
                        mSeries = empty;
                    }
                    else
                    {
                        mSeries = name;
                    }
                }
            }
            else if(0 == strncmp(buf, prog_id, strlen(prog_id)))
            {
            // Programme
                strtok(buf, delim);
                const char * programme = strtok(NULL, delim);
                const char * pa = strtok(NULL, delim);
                const char * director = strtok(NULL, delim);
                const char * producer = strtok(NULL, delim);
                if(NULL != producer)
                {
                    // Programme Name
                    if(0 == strcmp(programme, blank))
                    {
                        mProgramme = empty;
                    }
                    else
                    {
                        mProgramme = programme;
                    }
                    // Producer
                    if(0 == strcmp(producer, blank))
                    {
                        mProducer = empty;
                    }
                    else
                    {
                        mProducer = producer;
                    }
                    // Director
                    if(0 == strcmp(director, blank))
                    {
                        mDirector = empty;
                    }
                    else
                    {
                        mDirector = director;
                    }
                    // PA
                    if(0 == strcmp(pa, blank))
                    {
                        mPA = empty;
                    }
                    else
                    {
                        mPA = pa;
                    }
                }
            }
            else if(0 == strncmp(buf, seq_id, strlen(seq_id)))
            {
            // Sequence
                strtok(buf, delim);
                char * name = strtok(NULL, delim);
                char * start = strtok(NULL, delim);
                char * end = strtok(NULL, delim);
#if 0
            // use sequence id from file
                char * id = strtok(NULL, delim);
#else
            // replace sequence id
                strtok(NULL, delim);
                const char * id = GenerateSeqId(start);
#endif
                char * duration = strtok(NULL, delim);
                if(NULL != end)
                {
                    ScriptRef start_ref(start);
                    ScriptRef end_ref(end);
                    SequencePtr seq(new ::Sequence(name, start_ref, id));
#if 0
                    if(NULL != duration)
                    {
                        seq->TargetDuration(duration);
                    }
                    else
                    {
                        seq->TargetDuration("00:00:05:00"); // default value
                    }
#endif
                    mRecordingOrder.push_back(seq);
                }
            }
            else if(0 == strncmp(buf, take_id, strlen(take_id)))
            {
            // Take
                strtok(buf, delim);
                const char * number = strtok(NULL, delim);
                const char * date = strtok(NULL, delim);
                const char * in = strtok(NULL, delim);
                const char * out = strtok(NULL, delim);
                const char * comment = strtok(NULL, delim);
                const char * result = strtok(NULL, delim);
                if(NULL != result && mRecordingOrder.size() > 0)
                {
                    ::Sequence & sequence = * mRecordingOrder.back();
                    ::Take take;
                    take.Number(ACE_OS::atoi(number));
                    //take.Date(date);
                    take.In(Timecode(in));
                    take.Out(Timecode(out));
                    take.IsGood(0 == strcmp(result, good));
                    if(0 != strcmp(comment, blank))
                    {
                        take.Comment(comment);
                    }
                    sequence.AddTake(take);
                }
            }
#if 0
            else if(0 == strncmp(buf, recorded_source_id, strlen(recorded_source_id)))
            {
            // RecordedSource
                bool ok_so_far = true;
                ::Sequence & sequence = * mRecordingOrder.back();
                ok_so_far &= (sequence.TakeCount() > 0);

                strtok(buf, delim);
                char * name;
                char * description;
                ok_so_far &= (NULL != (name = strtok(NULL, delim)));
                ok_so_far &= (NULL != (description = strtok(NULL, delim)));
                if(ok_so_far)
                {
                    RecordedSource rsource;
                    if(0 != strcmp(name, blank))
                    {
                        rsource.Source().Name(name);
                    }
                    if(0 != strcmp(description, blank))
                    {
                        rsource.Source().Description(description);
                    }

                    ::Take & take = sequence.LastTake();
                    take.AddRecordedSource(rsource);
                }
            }
            else if(0 == strncmp(buf, clip_id, strlen(clip_id)))
            {
            // Clip
#if 1
// to read in new format files
                ::Sequence & sequence = * mRecordingOrder.back();
                ::Take & take = sequence.LastTake();
                ::RecordedSource & rsource = take.LastRecordedSource();

                strtok(buf, delim);
                char * type = strtok(NULL, delim);
                char * tape = strtok(NULL, delim);
                char * file = strtok(NULL, delim);
                char * in_timecode = strtok(NULL, delim);
                char * out_timecode = strtok(NULL, delim);
                char * format = strtok(NULL, delim);
                if(NULL != in_timecode)
                {
                    if(0 == strcmp(type, type_tape))
                    {
                        // tape
                        ::Recording tape_recording; 
                        //tape_recording.Type(Recording::TAPE);
                        tape_recording.Format(Recording::TAPE);
                        if(0 != strcmp(tape, blank))
                        {
                            tape_recording.TapeId(tape);
                        }

                        ::Clip tape_clip;
                        tape_clip.Recording(tape_recording);
                        rsource.AddClip(tape_clip);
                    }
                    else if(0 == strcmp(type, type_file))
                    {
                        // file
                        ::Recording file_recording;
                        //file_recording.Type(Recording::FILE);
                        file_recording.Format(Recording::DV);
                        if(0 != strcmp(file, blank))
                        {
                            file_recording.FileId(file);
                        }
                        if(0 != strcmp(in_timecode, blank))
                        {
                            file_recording.FileStartTimecode(in_timecode);
                        }
                        if(NULL != out_timecode && 0 != strcmp(out_timecode, blank))
                        {
                            file_recording.FileEndTimecode(out_timecode);
                        }
                        if(NULL != format && 0 != strcmp(format, blank))
                        {
                            if(0 == strcmp(format, format_sdi))
                            {
                                file_recording.Format(Recording::FILE);
                            }
                            else if(0 == strcmp(format, format_dv))
                            {
                                file_recording.Format(Recording::FILE);
                            }
                            else if(0 == strcmp(format, format_file))
                            {
                                file_recording.Format(Recording::FILE);
                            }
                        }

                        ::Clip file_clip;
                        file_clip.Recording(file_recording);
                        file_clip.Start(take.In() - file_recording.FileStartTimecode());
                        file_clip.Duration(take.Duration());
                        rsource.AddClip(file_clip);
                    }
                }
#else
// to read in old format files
                strtok(buf, delim);
                char * source = strtok(NULL, delim);
                char * tape = strtok(NULL, delim);
                char * file = strtok(NULL, delim);
                char * timecode = strtok(NULL, delim);
                //char * image = strtok(NULL, delim);
                if(NULL != timecode)
                {
                    ::Sequence & sequence = * mRecordingOrder.back();
                    RecordedSource rs;

                    if(0 != strcmp(source, blank))
                    {
                        rs.Source().Name(source);
                    }

                    // tape recording
                    ::Recording tape_recording; 
                    tape_recording.Type(Recording::TAPE);
                    tape_recording.Format(Recording::DIGIBETA);
                    if(0 != strcmp(tape, blank))
                    {
                        tape_recording.TapeId(tape);
                    }

                    ::Clip tape_clip;
                    tape_clip.Recording(tape_recording);
                    rs.AddClip(tape_clip);

                    // file recording
                    ::Recording file_recording;
                    //file_recording.Type(Recording::FILE);
                    file_recording.Format(Recording::DV);
                    if(0 != strcmp(file, blank))
                    {
                        file_recording.FileId(file);
                    }
                    if(0 != strcmp(timecode, blank))
                    {
                        file_recording.FileStartTimecode(timecode);
                    }

                    ::Take take = sequence.LastTake(); // should check that there is a take first
                    ::Clip file_clip;
                    file_clip.Recording(file_recording);
                    file_clip.Start(take.In() - file_recording.FileStartTimecode());
                    file_clip.Duration(take.Duration());
                    rs.AddClip(file_clip);
                    take.AddRecordedSource(rs);
                }
#endif
            }
#endif
        }
        if(SequenceCount() > 0)
        {
            mItemIndex = 0;
        }
        //CalcRecordedDuration();
        mIsEmpty = false;
    }
#endif
}

void CGalleryLoggerDoc::ArWrite(CArchive & ar, const char * value, bool prepend_delimiter)
{
    if(prepend_delimiter)
    {
        ar.WriteString(delim);
    }
    if(ACE_OS::strlen(value) == 0)
    {
        ar.WriteString(blank);
    }
    else
    {
        ar.WriteString(value);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerDoc diagnostics

#ifdef _DEBUG
void CGalleryLoggerDoc::AssertValid() const
{
    CDocument::AssertValid();
}

void CGalleryLoggerDoc::Dump(CDumpContext& dc) const
{
    CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerDoc commands

/**
Import sequence list from a ScriptWriter2 file.
We create one chunk to be recorded (our meaning of sequence)
for each sequence in the script.
*/
void CGalleryLoggerDoc::Import(const char * filename) 
{
    SwdFile swdfile;

    swdfile.SsfFile(filename);

#if 0 // needs updating to work with database
    unsigned int count = swdfile.OrderedSequence.size();
    for (unsigned int i = 0; i < count; ++i)
    {
        //int number = ACE_OS::atoi(swdfile.OrderedSequence[i].number.c_str());
        const char * script_seq_number = swdfile.OrderedSequence[i].number.c_str();
        ScriptRef start(script_seq_number);
        ScriptRef end(script_seq_number);
        const char * name = swdfile.OrderedSequence[i].name.c_str();
        const char * id_str = GenerateSeqId(script_seq_number);
        //SequencePtr seq(new ::Sequence(number, name, id_str));
        SequencePtr seq(new ::Sequence(name, start, id_str));
        mRecordingOrder.push_back(seq);
    }
    if(count > 0)
    {
        mItemIndex = 0;
    }
#else
    if (mpDb && mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        try
        {
            const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
            prodauto::Programme * prog = programme_vector[mProgrammeIndex];
            unsigned int count = swdfile.OrderedSequence.size();
            for (unsigned int i = 0; i < count; ++i)
            {
                prodauto::Item * item = prog->appendNewItem();
                item->description = swdfile.OrderedSequence[i].name;
                item->scriptSectionRefs.push_back(swdfile.OrderedSequence[i].number);
                mpDb->saveItem(item);
            }
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
#endif
    
    SetModifiedFlag();
    mIsEmpty = false;

    UpdateAllViews(NULL);
}

#if 0
void CGalleryLoggerDoc::ExportGoodTakeList(const char * filename)
{
    std::ofstream f(filename);
    if(f.is_open())
    {
        for(int s = 0; s < SequenceCount(); ++s)
        {
            ::Sequence & sequence = * mRecordingOrder[s];
            for(int t = 0; t < sequence.TakeCount(); ++t)
            {
                ::Take & take = sequence.Take(t);
                if(take.IsGood())
                {
                    // Recorded Sources
                    for(int rs = 0; rs < take.SourceCount(); ++rs)
                    {
                        ::RecordedSource & rec_source = take.RecordedSource(rs);

                        // Look for TAPE and FILE clips
                        int tape_clip_index = -1;
                        int file_clip_index = -1;
                        for(int c = 0; c < rec_source.ClipCount(); ++c)
                        {
                            ::Clip & clip = rec_source.Clip(c);
                            switch (clip.Recording().Format())
                            {
                            case Recording::TAPE:
                                tape_clip_index = c;
                                break;
                            case Recording::DV:
                            case Recording::SDI:
                                file_clip_index = c;
                                break;
                            }
                        }
                        // Now get info on FILE clips
                        if(file_clip_index != -1)
                        {
                            ::Clip & file_clip = rec_source.Clip(file_clip_index);
                            f << file_clip.Recording().FileId() << "\n";
                        }
                    }
                }
            }
        }
    }
}

void CGalleryLoggerDoc::ExportAle(const char * filename, int src) 
{
    std::ofstream f(filename);
    if(f.is_open() && src < mSources.size())
    {
        f << "Heading\n";
        f << "FIELD_DELIM\tTABS\n";
        f << "VIDEO_FORMAT\tPAL\n";
        f << "AUDIO_FORMAT\t48kHz\n";
        f << "FPS\t25\n\n";

        f << "Column\n";
        f << "Name\tTracks\tStart\tEnd\tMark IN\tMark OUT\tTape\tShoot Date\tDESCRIPT\tCOMMENTS\n\n";

        f << "Data\n";
        for(int s = 0; s < SequenceCount(); ++s)
        {
            ::Sequence & sequence = * mRecordingOrder[s];
            for(int t = 0; t < sequence.TakeCount(); ++t)
            {
                ::Take & take = sequence.Take(t);
                if(take.IsGood())
                {
                    // Recorded Sources
                    for(int rs = 0; rs < take.SourceCount(); ++rs)
                    {
                        ::RecordedSource & rec_source = take.RecordedSource(rs);

                        // Pick out the required source
                        if(0 == ACE_OS::strcmp(rec_source.Source().Name(),
                            mSources[src].Name()))
                        {

                            // Look for TAPE and FILE clips
                            int tape_clip_index = -1;
                            int file_clip_index = -1;
                            for(int c = 0; c < rec_source.ClipCount(); ++c)
                            {
                                ::Clip & clip = rec_source.Clip(c);
                                switch (clip.Recording().Format())
                                {
                                case Recording::TAPE:
                                    tape_clip_index = c;
                                    break;
                                case Recording::DV:
                                case Recording::SDI:
                                    file_clip_index = c;
                                    break;
                                }
                            }
                            if(tape_clip_index != -1)
                            {
                                ::Clip & tape_clip = rec_source.Clip(tape_clip_index);

                                if(Recording::TAPE == tape_clip.Recording().Format())
                                {
                                    // Name
                                    f << "S" << sequence.UniqueId()
                                        << "_T" << take.Number()
                                        << "_" << rec_source.Source().Name()
                                        //<< ".dv\t";
                                        << "\t";

                                    // Tracks
                                    f << "VA1A2A3A4\t";

                                    // Timing - We do this differently depending
                                    // on whether we have a file recording.
                                    Timecode in = take.In();
                                    Timecode out = take.Out();
                                    Timecode start;
                                    Timecode end;
                                    if(file_clip_index != -1)
                                    {
                                        ::Clip & file_clip = rec_source.Clip(file_clip_index);
                                        start = file_clip.Recording().FileStartTimecode();
                                        end = file_clip.Recording().FileEndTimecode();
                                    }
                                    else
                                    {
                                        Duration handle = "00:00:01:00";
                                        start = in - handle;
                                        end = out + handle;
                                    }
                                    // Start, End
                                    f << start.Text() << "\t";
                                    f << end.Text() << "\t";


                                    // In, Out
                                    f << in.Text() << "\t";
                                    f << out.Text() << "\t";

                                    // Tape
                                    if(tape_clip_index != -1)
                                    {
                                        ::Clip & tape_clip = rec_source.Clip(tape_clip_index);
                                        f /*<< rec_source.Source().Name() << " - "*/
                                            << tape_clip.Recording().TapeId() << "\t";
                                    }
                                    else
                                    {
                                        f << "none\t";
                                    }

                                    // Shoot date
                                    f << take.Date() << "\t";

                                    // Description
                                    f << sequence.Name() << "\t";

                                    // Comments
                                    f << take.Comment() << "\n";
                                }
                            } // looking for TAPE clip
                        } // selecting required source
                    }
                } //if(take.IsGood())
            }
        }
    }
}
#endif

/**
Add a new item after the current one.
Depending on type (e.g. pickup), copy parameters of previous item.
Move to the new item.
*/
void CGalleryLoggerDoc::AddNewItem(ItemTypeEnum type, bool update_view)
{
    if (mpDb && mSeriesIndex >= 0 && mProgrammeIndex >= 0 && mItemIndex >= 0)
    {
        try
        {
            const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
            prodauto::Programme * prog = programme_vector[mProgrammeIndex];
            const std::vector<prodauto::Item *> & item_vector = prog->getItems();
            prodauto::Item * new_item;
            unsigned int new_index = mItemIndex + 1;
            if (new_index < item_vector.size())
            {
                new_item = prog->insertNewItem(new_index);
            }
            else
            {
                new_item = prog->appendNewItem();
            }

            prodauto::Item * current_item = item_vector[mItemIndex];
            switch (type)
            {
            case PICKUP:
                new_item->description = "Pickup";
                new_item->scriptSectionRefs = current_item->scriptSectionRefs;
                break;
            case REPEAT:
                new_item->description = current_item->description;
                new_item->scriptSectionRefs = current_item->scriptSectionRefs;
                break;
            case BLANK:
            default:
                new_item->description = "New Item";
                break;
            }

            mpDb->saveItem(new_item);

            // Move to new item
            this->SelectItem(new_index, false);
        }
        catch (...)
        {
        }
    }

    if (update_view)
    {
        UpdateAllViews(NULL);
    }
}

void CGalleryLoggerDoc::DeleteItem()
{
    if (mpDb && mSeriesIndex >= 0 && mProgrammeIndex >= 0 && mItemIndex >= 0)
    {
        try
        {
            const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
            prodauto::Programme * prog = programme_vector[mProgrammeIndex];
            const std::vector<prodauto::Item *> & item_vector = prog->getItems();
            if (mItemIndex >= 0 && mItemIndex < item_vector.size())
            {
                prodauto::Item * item = item_vector[mItemIndex];
                mpDb->deleteItem(item);
                delete item;
                if (mItemIndex > 0)
                {
                    --mItemIndex;
                }
            }
        }
        catch (const prodauto::DBException &)
        {
        }
    }

    SetModifiedFlag();
    UpdateAllViews(NULL);
}

void CGalleryLoggerDoc::DeleteContents() 
{
    // ACE_Strong_Bound_Ptr takes care of cleaning up
    // all those sequences allocated with new.
    //mRecordingOrder.clear();
    mItemIndex = -1;
    
    mSeries = "";
    mProgramme = "";
    mProducer = "";
    mDirector = "";
    mPA = "";

    //mSources.clear();

    mIsEmpty = true;

    // Call base class
    CDocument::DeleteContents();
}

#if 0
void CGalleryLoggerDoc::CalcRecordedDuration()
{
// We can never be completely accurate with this when there is more than
// one good take of a sequence but our strategy is as follows:
// Include the duration of the first good take.

    int s, t;
    Timecode result;
    bool first_good_take;
    bool pickup;

    for(s = 0; s < SequenceCount(); ++s)
    {
        ::Sequence & sequence = * mRecordingOrder[s];
        first_good_take = true;
        pickup = false;
        for(t = 0; t < sequence.TakeCount(); ++t)
        {
            ::Take & take = sequence.Take(t);
            if(take.IsGood())
            {
                if(first_good_take)
                {
                    result += take.Duration();
                    first_good_take = false;
                }
            }
        }
    }

    mRecordedDuration = result;
}
#endif

const char * CGalleryLoggerDoc::ClipFilename(const char * src, const char * seq_id, int take, const char * suffix)
{
    mClipFilename = mSeries;
    mClipFilename += '_';
    mClipFilename += mProgramme;
    mClipFilename += '_';
    mClipFilename += src;
    mClipFilename += '_';
    char tmp[20];
    ACE_OS::sprintf(tmp, "s%st%03d", seq_id, take);
    mClipFilename += tmp;
    mClipFilename += suffix;

    return mClipFilename.c_str();
}

#if 0
const char * CGalleryLoggerDoc::GenerateSeqId(const char * ref)
{
    // Get numeric sequence ref - will be zero if ref not a number
    int ref_no = ACE_OS::atoi(ref);

    // Generate the "sub-index" number
    int sub_index = 0;
    for(int i = 0; i < SequenceCount(); ++i)
    {
        if(ACE_OS::atoi(mRecordingOrder[i]->Start().Value()) == ref_no)
        {
            ++ sub_index;
        }
    }

    // Generate id string from sequence number and sub-index
    char id_str[10];
    ACE_OS::sprintf(id_str, "%03d-%02d", ref_no, sub_index);
    mSeqId = id_str;

    return mSeqId.c_str();
}

void CGalleryLoggerDoc::MakeProgrammeOrder()
{
    // Create programme order vector
    mProgrammeOrder = mRecordingOrder;

    // Sort on sequence number
    std::sort(mProgrammeOrder.begin(), mProgrammeOrder.end(), CompareSequenceNumbers());

    //for(int i = 0; i < SequenceCount(); ++i)
    //{
    //  mProgrammeOrder[i]->ProgrammeIndex(i);
    //}
}
#endif

/**
Save a copy of log without changing document title.
*/
void CGalleryLoggerDoc::SaveLogTo(const char * pathname)
{
    // Decode the path elements.
    char tmp_path[255];
    ACE_OS::strcpy(tmp_path, pathname);
    const char * delim = "\\";
    const char * path_element;
    std::vector<std::string> path_elements;
    char * str = tmp_path;
    while(NULL != (path_element = ACE_OS::strtok(str, delim)))
    {
        path_elements.push_back(path_element);
        str = NULL;
    }

    // Add elements to path and check that each folder exists, creating
    // it if necesssary.
    ACE_stat tmpstat;
    std::string trial_path;
    for (unsigned int i = 0; i < path_elements.size() - 1; ++i)
    {
        trial_path += path_elements[i];
        if(0 == ACE_OS::stat(trial_path.c_str(), &tmpstat))
        {
            // folder exists
        }
        else if(ENOENT == ACE_OS::last_error())
        {
            // creating folder
            ACE_OS::mkdir(trial_path.c_str());
        }
        else
        {
            // stat failed
        }
        trial_path += delim;
    }

    // Now save logfile
    try
    {
        CFile file(pathname, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite);
        CArchive ar(&file, CArchive::store);
        Serialize(ar);
    }
    catch(...)
    {
        // failed
    }
}

/**
Do a backup save to temporary folder
*/
void CGalleryLoggerDoc::AutoSave()
{
    // Set folder for backups
    std::vector<std::string> fullpath;
    fullpath.push_back("C:\\TEMP");
    fullpath.push_back("\\GalleryLogger");

    // Make sure folder exists
    ACE_stat tmpstat;
    std::string pathname;
    for (unsigned int i = 0; i < fullpath.size(); ++i)
    {
        pathname += fullpath[i];
        if(0 == ACE_OS::stat(pathname.c_str(), &tmpstat))
        {
            // folder exists
        }
        else if(ENOENT == ACE_OS::last_error())
        {
            // creating folder
            ACE_OS::mkdir(pathname.c_str());
        }
        else
        {
            // stat failed
        }
    }

    // Set filename

    // get filename minus .log extension
    std::string basename = GetTitle();
    size_t n = basename.find(".log");
    basename = basename.substr(0, n);
    basename += "_backup";
    char tmp[8];
    basename += ACE_OS::itoa(mBackupIndex, tmp, 10);
    basename += ".log";

    pathname += "\\";
    pathname += basename;

    mBackupIndex = ++mBackupIndex % 10;

    // Save logfile
    try
    {
        CFile file(pathname.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite);
        CArchive ar(&file, CArchive::store);
        Serialize(ar);
    }
    catch(...)
    {
        // failed
    }
}

// Operations on database

const std::map<long, std::string> & CGalleryLoggerDoc::RecordingLocations()
{
    mRecordingLocations.clear();
    if (mpDb)
    {
        try
        {
            mRecordingLocations = mpDb->loadLiveRecordingLocations();
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    return mRecordingLocations;
}

void CGalleryLoggerDoc::RecordingLocation(const char * loc)
{
    std::map<long, std::string>::const_iterator it;
    bool found = false;
    mRecordingLocation = 0;
    for (it = mRecordingLocations.begin(); !found && it != mRecordingLocations.end(); ++it)
    {
        if (it->second == loc)
        {
            found = true;
            mRecordingLocation = it->first;
        }
    }
}

void CGalleryLoggerDoc::SelectSeries(const char * s, bool update_view)
{
    mSeriesIndex = -1;
    for (unsigned int i = 0; mSeriesIndex == -1 && i < mSeriesVector.size(); ++i)
    {
        if (mSeriesVector[i]->name == s)
        {
            mSeriesIndex = i;
            try
            {
                mpDb->loadProgrammesInSeries(mSeriesVector[i]);
            }
            catch(...)
            {
            }
        }
    }
    SelectProgramme("", false);

    if (update_view)
    {
        UpdateAllViews(NULL);
    }
}

const char * CGalleryLoggerDoc::CurrentSeriesName()
{
    const char * s;
    if (mSeriesIndex >=0 && mSeriesIndex < mSeriesVector.size())
    {
        s = mSeriesVector[mSeriesIndex]->name.c_str();
    }
    else
    {
        s = "";
    }
    return s;
}

const char * CGalleryLoggerDoc::SeriesName(unsigned int i)
{
    const char * s;
    if (i < mSeriesVector.size())
    {
        s = mSeriesVector[i]->name.c_str();
    }
    else
    {
        s = "";
    }
    return s;
}

unsigned int CGalleryLoggerDoc::ProgrammeCount()
{
    int count = 0;
    if (mSeriesIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        count = programme_vector.size();
    }
    return count;
}

void CGalleryLoggerDoc::SelectProgramme(const char * s, bool update_view)
{
    mProgrammeIndex = -1;
    mItemIndex = -1;
    if (mSeriesIndex != -1)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        for (unsigned int i = 0; mProgrammeIndex == -1 && i < programme_vector.size(); ++i)
        {
            prodauto::Programme * prog = programme_vector[i];
            if (prog->name == s)
            {
                // Found the programme
                mProgrammeIndex = i;
                mpDb->loadItemsInProgramme(prog);

                // Load takes in all items so we can provide HasGoodTake method.
                const std::vector<prodauto::Item *> item_vector = prog->getItems();
                for (unsigned int item_i = 0; item_i < item_vector.size(); ++item_i)
                {
                    mpDb->loadTakesInItem(item_vector[item_i]);
                }
                if (item_vector.size() > 0)
                {
                    // Select first item.
                    SelectItem(0, false);
                }
            }
        }
    }

    if (update_view)
    {
        UpdateAllViews(NULL);
    }
}

const char * CGalleryLoggerDoc::ProgrammeName(unsigned int i)
{
    const char * s = "";
    if (mSeriesIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        if (i < programme_vector.size())
        {
            s = programme_vector[i]->name.c_str();
        }
    }
    return s;
}

const char * CGalleryLoggerDoc::CurrentProgrammeName()
{
    const char * s = "";
    if (mSeriesIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        if (mProgrammeIndex >= 0 && mProgrammeIndex < programme_vector.size())
        {
            s = programme_vector[mProgrammeIndex]->name.c_str();
        }
    }
    return s;
}

unsigned int CGalleryLoggerDoc::ItemCount()
{
    int count = 0;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        count = item_vector.size();
    }
    return count;
}

void CGalleryLoggerDoc::SelectItem(int i, bool update_view)
{
    //mItemIndex = -1;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0 && i >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        if (i < item_vector.size())
        {
            mItemIndex = i;
            // NB. Takes in item will already have been loaded by SelectProgramme.
        }
    }

    if (update_view)
    {
        UpdateAllViews(NULL);
    }
}

void CGalleryLoggerDoc::InsertItem(const std::string & descript)
{
    if (mpDb && mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        try
        {
            const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
            prodauto::Programme * prog = programme_vector[mProgrammeIndex];
            const std::vector<prodauto::Item *> & item_vector = prog->getItems();
            prodauto::Item * item;
            int new_index = mItemIndex + 1;
            if (new_index < item_vector.size())
            {
                item = prog->insertNewItem(new_index);
            }
            else
            {
                item = prog->appendNewItem();
            }
            item->description = descript;

            mpDb->saveItem(item);
        }
        catch (...)
        {
        }

        UpdateAllViews(NULL);
    }
}

/*
    Now follow various methods for accessing an item.
*/

/**
Return a pointer to the actual item object so that you can access
or modify the data in any way.
*/
prodauto::Item * CGalleryLoggerDoc::Item(unsigned int i)
{
    prodauto::Item * item = 0;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        if (i < item_vector.size())
        {
            item = item_vector[i];
        }
    }
    return item;
}

const char * CGalleryLoggerDoc::ItemDescription(unsigned int i)
{
    const char * s = "";
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        if (i < item_vector.size())
        {
            s = item_vector[i]->description.c_str();
        }
    }
    return s;
}

/**
Returnn script-refs as a comma-separated list.
*/
std::string CGalleryLoggerDoc::ItemScriptRefs(unsigned int i)
{
    prodauto::Item * item = this->Item(i);
    std::string s;
    if (item)
    {
        for (unsigned int ref_i = 0; ref_i < item->scriptSectionRefs.size(); ++ref_i)
        {
            if (ref_i != 0)
            {
                s += ", ";
            }
            s += item->scriptSectionRefs[ref_i];
        }
    }

    return s;
}


bool CGalleryLoggerDoc::ItemHasGoodTake(unsigned int item_i)
{
    bool answer = false;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        if (item_i < item_vector.size())
        {
            const std::vector<prodauto::Take *> & take_vector = item_vector[item_i]->getTakes();
            for (unsigned int take_i = 0; take_i < take_vector.size(); ++take_i)
            {
                prodauto::Take * take = take_vector[take_i];
                if (take->result == GOOD_TAKE_RESULT)
                {
                    answer = true;
                    break;
                }
            }
        }
    }
    return answer;
}

void CGalleryLoggerDoc::SaveItem(prodauto::Item * item, bool update_view)
{
    try
    {
        mpDb->saveItem(item);
    }
    catch(const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }

    if (update_view)
    {
        UpdateAllViews(NULL);
    }
}

// Database Take operations

unsigned int CGalleryLoggerDoc::TakeCount()
{
    int count = 0;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0 && mItemIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        const std::vector<prodauto::Take *> & take_vector = item_vector[mItemIndex]->getTakes();
        count = take_vector.size();
    }
    return count;
}

prodauto::Take * CGalleryLoggerDoc::Take(unsigned int i)
{
    prodauto::Take * ptr = 0;
    if (mSeriesIndex >= 0 && mProgrammeIndex >= 0 && mItemIndex >= 0)
    {
        const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
        const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
        const std::vector<prodauto::Take *> & take_vector = item_vector[mItemIndex]->getTakes();
        if (i < take_vector.size())
        {
            ptr = take_vector[i];
        }
    }
    return ptr;
}


void CGalleryLoggerDoc::AddTake(const ::Take & take)
{
    const std::vector<prodauto::Programme *> & programme_vector = mSeriesVector[mSeriesIndex]->getProgrammes();
    const std::vector<prodauto::Item *> & item_vector = programme_vector[mProgrammeIndex]->getItems();
    const std::vector<prodauto::Take *> & take_vector = item_vector[mItemIndex]->getTakes();
    prodauto::Take * db_take = item_vector[mItemIndex]->createTake();

    take.CopyTo(db_take);
    // And in case these parameters weren't set in the incoming take
    db_take->number = take_vector.size();
    db_take->recordingLocation = mRecordingLocation;

    if (mpDb)
    {
        try
        {
            mpDb->saveTake(db_take);
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }

    UpdateAllViews(NULL, CGalleryLoggerDoc::NOT_SEQUENCE_LIST);
}

void CGalleryLoggerDoc::ModifyTake(int index, const ::Take & take)
{
    prodauto::Take * db_take = this->Take(index);
    if (db_take)
    {
        take.CopyTo(db_take);

        if (mpDb)
        {
            try
            {
                mpDb->saveTake(db_take);
            }
            catch(const prodauto::DBException & dbe)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
            }
        }

        UpdateAllViews(NULL, CGalleryLoggerDoc::NOT_SEQUENCE_LIST);
    }
}


void CGalleryLoggerDoc::RecorderName(const char * name)
{
    // Load Recorder of that name.
    prodauto::Recorder * rec = 0;
    if(mpDb)
    {
        try
        {
            rec = mpDb->loadRecorder(name);
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("Loaded Recorder \"%C\"\n"), rec->name.c_str()));
        }
        catch(const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
    std::auto_ptr<prodauto::Recorder> recorder(rec); // so it gets deleted

    // Get RecorderConfig
    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    // Get RecorderInputConfigs
    for(unsigned int i = 0; rc && i < rc->recorderInputConfigs.size(); ++i)
    {
        prodauto::RecorderInputConfig * ric =
            rc->getInputConfig(i + 1); // Index starts from 1
        for(unsigned int j = 0; ric && j < ric->trackConfigs.size(); ++j)
        {
            prodauto::RecorderInputTrackConfig * ritc = ric->trackConfigs[j];
            prodauto::SourceConfig * sc = ritc->sourceConfig;
            prodauto::SourceTrackConfig * stc = sc->getTrackConfig(ritc->sourceTrackID);

            // Here do something with the information

            //source.package_name = CORBA::string_dup(sc->name.c_str());
            //source.track_name = CORBA::string_dup(stc->name.c_str());
        }
    }
}


