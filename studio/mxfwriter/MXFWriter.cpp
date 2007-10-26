/*
 * $Id: MXFWriter.cpp,v 1.1 2007/09/11 14:08:44 stuart_hc Exp $
 *
 * Writes essence data to MXF files
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
 
#include <cassert>
#include <sstream>
#include <cerrno>

#include "MXFWriter.h"
#include "MXFWriterException.h"
#include <Database.h>
#include <Utilities.h>
#include <Logging.h>


extern "C"
{
#include <write_avid_mxf.h>
};


using namespace std;
using namespace prodauto;



#define CHECK_SUCCESS(cmd) \
    if (!(cmd)) \
    { \
        PA_LOGTHROW(MXFWriterException, ("'%s' failed", #cmd)); \
    }


// track count is used to generate the track names in the material package
typedef struct
{
    uint32_t pictureTrackCount;
    uint32_t soundTrackCount;
} TrackCount;

    
    
static void convertUMID(prodauto::UMID& in, mxfUMID& out)
{
    memcpy(&out, &in, 32);
}

static void convertTimestamp(prodauto::Timestamp& in, mxfTimestamp& out)
{
    memcpy(&out, &in, 8);
}

static void convertRational(prodauto::Rational& in, mxfRational& out)
{
    memcpy(&out, &in, 8);
}

static string createFilename(string filenamePrefix, bool isPicture, uint32_t trackID, uint32_t inputTrackID)
{
    stringstream filename;
    
    filename << filenamePrefix << "_" << inputTrackID << "_";
    
    if (isPicture)
    {
        filename << "v";
    }
    else
    {
        filename << "a";
    }
    filename << trackID << ".mxf";
    
    return filename.str();
}
 
static string createCompleteFilename(string filePath, string filename)
{
    string completeFilename = filePath;
    if (filePath.size() > 0)
    {
#if defined(_WIN32)
        completeFilename.append("\\");
#else
        completeFilename.append("/");
#endif
    }
    completeFilename.append(filename);
    
    return completeFilename;
}

static void moveFile(string from, string to)
{
    if (rename(from.c_str(), to.c_str()) != 0)
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to move file from %s to %s (error = %d)", 
            from.c_str(), to.c_str(), errno));
    }
}


OutputPackage::OutputPackage()
: sourcePackage(0), materialPackage(0)
{}

OutputPackage::~OutputPackage()
{
    if (ownPackages)
    {
        delete materialPackage;
        
        vector<SourcePackage*>::const_iterator iter;
        for (iter = filePackages.begin(); iter != filePackages.end(); iter++)
        {
            delete *iter;
        }
    }
    
    if (clipWriter != 0)
    {
        // files will be removed
        abort_writing(&clipWriter, 1);
    }
    
    // sourcePackage is not owned by OutputPackage
}

void OutputPackage::updateDuration(uint32_t materialTrackID, uint32_t numSamples)
{
    // get material package track
    prodauto::Track* materialTrack = materialPackage->getTrack(materialTrackID);
    if (materialTrack == 0)
    {
        PA_LOGTHROW(MXFWriterException, ("Missing material package track %u", materialTrackID));
    }
    
    // get referenced file package track
    SourcePackage* filePackage = 0;
    prodauto::Track* fileTrack = 0;
    vector<SourcePackage*>::const_iterator iter;
    for (iter = filePackages.begin(); iter != filePackages.end(); iter++)
    {
        if ((*iter)->uid == materialTrack->sourceClip->sourcePackageUID)
        {
            filePackage = *iter;
            fileTrack = filePackage->getTrack(materialTrack->sourceClip->sourceTrackID);
            if (fileTrack == 0)
            {
                PA_LOGTHROW(MXFWriterException, ("Missing file package track %u", materialTrack->sourceClip->sourceTrackID));
            }
            break;
        }
    }
    if (filePackage == 0)
    {
        PA_LOGTHROW(MXFWriterException, ("Missing file package referenced by material package track %u", materialTrackID));
    }
    
    // update file package track duration (numSamples is always in file package edit rate units)
    fileTrack->sourceClip->length += numSamples;
    
    // update material package track duration, taking into account any differences in edit rate
    if (materialTrack->editRate == fileTrack->editRate)
    {
        materialTrack->sourceClip->length += numSamples;
    }
    else
    {
        double factor = materialTrack->editRate.numerator * fileTrack->editRate.denominator /
            (double)(materialTrack->editRate.denominator * fileTrack->editRate.numerator);
        materialTrack->sourceClip->length += (int64_t)(numSamples * factor + 0.5);
    }
    
}
    



MXFTrackWriter::MXFTrackWriter()
: clipWriter(0), outputPackage(0)
{}

MXFTrackWriter::~MXFTrackWriter()
{
    // outputPackage is not owned by MXFTrackWriter 
}

// TODO: ownership of packages and who delete them
MXFWriter::MXFWriter(MaterialPackage* materialPackage, 
    vector<SourcePackage*>& filePackages, 
    SourcePackage* tapeSourcePackage, bool ownPackages,
    int resolutionID, Rational imageAspectRatio, uint8_t audioQuantizationBits, 
    int64_t startPosition, 
    string creatingFilePath, string destinationFilePath, string failuresFilePath, 
    string filenamePrefix,
    vector<UserComment> userComments,
    string avidProjectName)
: _isComplete(false), _creatingFilePath(creatingFilePath), _destinationFilePath(destinationFilePath),
  _failuresFilePath(failuresFilePath), _filenamePrefix(filenamePrefix)
{
    map<MaterialPackage*, TrackCount> trackCounts;
    std::string filename;
    
    Timestamp now = generateTimestampNow(); // give material and all file packages the same creationDate
    
    // create a MXFTrackWriter for each connected and non-masked input track
    vector<Track*>::const_iterator iterMaterialTracks;
    for (iterMaterialTracks = materialPackage->tracks.begin();
        iterMaterialTracks != materialPackage->tracks.end(); 
        iterMaterialTracks++)
    {
        Track* materialTrack = *iterMaterialTracks;
        
        if (!materialTrack->sourceClip)
        {
            continue;
        }

        uint32_t trackIndex = materialTrack->id;
        
        SourcePackage* filePackage = getSourcePackage(filePackages, 
            materialTrack->sourceClip->sourcePackageUID,
            materialTrack->sourceClip->sourceTrackID);
            
        if (filePackage)
        {
            Track* filePackageTrack;
            if (filePackage->tracks.size() != 1)
            {
                PA_LOGTHROW(MXFWriterException, ("Expecting 1 track in the file source package, but found %d", filePackage->tracks.size()));
            }
            filePackageTrack = *filePackage->tracks.begin();
            
            // create a track writer        
            MXFTrackWriter* trackWriter = new MXFTrackWriter();
            _trackWriters.insert(pair<uint32_t, MXFTrackWriter*>(trackIndex, trackWriter));

            // get or create the output package associated with the source package for the track
            OutputPackage* outputPackage = getOutputPackage(tapeSourcePackage);
            if (outputPackage == 0)
            {
                outputPackage = new OutputPackage();
                _outputPackages.push_back(outputPackage);

                outputPackage->ownPackages = ownPackages;                
                outputPackage->sourcePackage = tapeSourcePackage;
                outputPackage->materialPackage = materialPackage;
            }
            trackWriter->outputPackage = outputPackage;
            
            // add file package to output package
            trackWriter->outputPackage->filePackages.push_back(filePackage);
            
            
            // get the source package track
            prodauto::Track* sourceTrack = outputPackage->sourcePackage->getTrack(filePackageTrack->sourceClip->sourceTrackID);
            if (sourceTrack == 0)
            {
                PA_LOGTHROW(MXFWriterException, ("Failed to get source package track associated with recorder input track"));
            }

            
            // create complete filename
            filename = createFilename(filenamePrefix, sourceTrack->dataDef == PICTURE_DATA_DEFINITION, 
                sourceTrack->id, trackIndex);
            _filenames.insert(pair<UMID, string>(filePackage->uid, filename));
            _filenames2.insert(pair<uint32_t, string>(trackIndex, filename));
            FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
            if (fileDesc == 0)
            {
                PA_LOGTHROW(MXFWriterException, ("File Source Package descriptor is not a FileEssenceDescriptor"));
            }
            fileDesc->fileLocation = createCompleteFilename(creatingFilePath, filename);
            
            
            // record map input track ID to material package track ID
            trackWriter->inputToMPTrackMap.insert(pair<uint32_t, uint32_t>(trackIndex, materialTrack->id));
            
            
            // set the status to not (yet) successfull
            _filePackageToInputTrackMap.insert(pair<UMID, uint32_t>(filePackage->uid, trackIndex));
            _status.insert(pair<uint32_t, bool>(trackIndex, false));
        }
    }
    
    // create a clip writer for each output package
    vector<OutputPackage*>::const_iterator iterOutPackages;
    for (iterOutPackages = _outputPackages.begin(); iterOutPackages != _outputPackages.end(); iterOutPackages++)
    {
        OutputPackage* outputPackage = *iterOutPackages;
        
        // create the clip writer        
        PackageDefinitions* packageDefinitions = NULL;
        outputPackage->clipWriter = NULL;
        try
        {
            mxfUMID umid;
            mxfTimestamp timestamp;
            mxfRational editRate;
            
            // map to MXF package definitions        
            
            CHECK_SUCCESS(create_package_definitions(&packageDefinitions));
            
            // map the source package
            convertUMID(outputPackage->sourcePackage->uid, umid);
            convertTimestamp(outputPackage->sourcePackage->creationDate, timestamp);
            CHECK_SUCCESS(create_tape_source_package(packageDefinitions, &umid, 
                outputPackage->sourcePackage->name.c_str(), &timestamp));
                
            // map the source package tracks
            vector<prodauto::Track*>::const_iterator iterTrack;
            for (iterTrack = outputPackage->sourcePackage->tracks.begin();
                iterTrack != outputPackage->sourcePackage->tracks.end();
                iterTrack++)
            {
                prodauto::Track* track = *iterTrack;

                ::Track* mxfTrack;

                convertUMID(track->sourceClip->sourcePackageUID, umid);
                convertRational(track->editRate, editRate);
                CHECK_SUCCESS(create_track(packageDefinitions->tapeSourcePackage, track->id, 
                    track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, track->sourceClip->sourceTrackID, 
                    track->sourceClip->position, track->sourceClip->length,
                    &mxfTrack));
            }
                
            
            // map the material package
            convertUMID(outputPackage->materialPackage->uid, umid);
            convertTimestamp(outputPackage->materialPackage->creationDate, timestamp);
            CHECK_SUCCESS(create_material_package(packageDefinitions, &umid, 
                outputPackage->materialPackage->name.c_str(), &timestamp));

            // map the material package tracks
            for (iterTrack = outputPackage->materialPackage->tracks.begin();
                iterTrack != outputPackage->materialPackage->tracks.end();
                iterTrack++)
            {
                prodauto::Track* track = *iterTrack;

                ::Track* mxfTrack;

                convertUMID(track->sourceClip->sourcePackageUID, umid);
                convertRational(track->editRate, editRate);
                CHECK_SUCCESS(create_track(packageDefinitions->materialPackage, track->id, 
                    track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, track->sourceClip->sourceTrackID, 
                    track->sourceClip->position, track->sourceClip->length,
                    &mxfTrack));
            }
                
            // add the user comments to the material package
            vector<UserComment>::const_iterator iterUC;
            for (iterUC = userComments.begin(); iterUC != userComments.end(); iterUC++)
            {
                const UserComment& userComment = *iterUC;
                if (userComment.name.size() == 0)
                {
                    PA_LOGTHROW(MXFWriterException, ("Invalid zero length user comment name"));
                }
                
                // add comment to material package destined for the mxf file
                CHECK_SUCCESS(add_user_comment(packageDefinitions->materialPackage, 
                    userComment.name.c_str(), userComment.value.c_str()));

                // add comment to material package destined for the database
                outputPackage->materialPackage->addUserComment(userComment.name, userComment.value);
            }
            
            
            // map the file packages
            vector<SourcePackage*>::const_iterator iterFilePackages;
            for (iterFilePackages = outputPackage->filePackages.begin(); 
                iterFilePackages != outputPackage->filePackages.end(); 
                iterFilePackages++)
            {
                SourcePackage* filePackage = *iterFilePackages;
                
                assert(filePackage->tracks.size() == 1);
                prodauto::Track* fileTrack = filePackage->tracks.back();

                // map the file package
                ::Package* mxfFilePackage;
                convertUMID(filePackage->uid, umid);
                convertTimestamp(filePackage->creationDate, timestamp);
                FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
                assert(fileDesc != 0);
                if (fileTrack->dataDef == PICTURE_DATA_DEFINITION)
                {
                    switch (resolutionID)
                    {
                        case UNC_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                UncUYVY, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case DV25_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                DVBased25, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case DV50_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                DVBased50, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG21_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res21;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG31_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res31;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG101_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res101;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG151S_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res151s;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG201_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res201;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG101M_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res101m;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX30_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX30, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX40_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX40, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX50_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX50, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        default:
                            PA_LOGTHROW(MXFWriterException, ("Unsupported resolution %d", resolutionID));
                            break;
                    }
                }
                else
                {
                    EssenceInfo essenceInfo;
                    essenceInfo.pcmInfo.bitsPerSample = audioQuantizationBits;
                    CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, filePackage->name.c_str(), 
                        &timestamp, fileDesc->fileLocation.c_str(), 
                        PCM, &essenceInfo, &mxfFilePackage));
                }
        
                // map the file package track
                ::Track* mxfTrack;
                convertUMID(fileTrack->sourceClip->sourcePackageUID, umid);
                convertRational(fileTrack->editRate, editRate);
                CHECK_SUCCESS(create_track(mxfFilePackage, fileTrack->id, 
                    0 /* number is ignored */, fileTrack->name.c_str(), 
                    fileTrack->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, fileTrack->sourceClip->sourceTrackID, 
                    fileTrack->sourceClip->position, fileTrack->sourceClip->length,
                    &mxfTrack));
            }
            
    
            // create the clip writer
            // TODO: don't hard code PAL_25i and drop frame flag
            mxfRational mxfAspectRatio;
            convertRational(imageAspectRatio, mxfAspectRatio); 
            if (!create_clip_writer(avidProjectName.size() > 0 ? avidProjectName.c_str() : 0, 
                PAL_25i, mxfAspectRatio, 0 , 1, 
                packageDefinitions, &outputPackage->clipWriter))
            {
                PA_LOGTHROW(MXFWriterException, ("Failed to create clip writer"));
            }
            
            free_package_definitions(&packageDefinitions);
        }
        catch (...)
        {
            free_package_definitions(&packageDefinitions);
            if (outputPackage->clipWriter != NULL)
            {
                abort_writing(&outputPackage->clipWriter, 1);
            }
            throw;
        }

        
        // set the clip writer in each input track writer that is part of the output package
        map<uint32_t, MXFTrackWriter*>::const_iterator iterWriters;
        for (iterWriters = _trackWriters.begin(); iterWriters != _trackWriters.end(); iterWriters++)
        {
            MXFTrackWriter* trackWriter = (*iterWriters).second;
            
            if (trackWriter->outputPackage == outputPackage)
            {
                trackWriter->clipWriter = outputPackage->clipWriter;
            }
        }
    }
}
    


MXFWriter::MXFWriter(RecorderConfig* recorderConfig, uint32_t inputConfigIndex, 
    int resolutionID, Rational imageAspectRatio, uint8_t audioQuantizationBits, 
    uint32_t inputMask, 
    int64_t startPosition, 
    string creatingFilePath, string destinationFilePath, string failuresFilePath, 
    string filenamePrefix,
    vector<UserComment> userComments,
    string avidProjectName)
: _isComplete(false), _creatingFilePath(creatingFilePath), _destinationFilePath(destinationFilePath),
  _failuresFilePath(failuresFilePath), _filenamePrefix(filenamePrefix)
{
    const TrackCount startTrackCount = {0, 0};
    map<MaterialPackage*, TrackCount> trackCounts;
    std::string filename;

    Timestamp now = generateTimestampNow(); // give material and all file packages the same creationDate
    
    RecorderInputConfig* inputConfig = recorderConfig->getInputConfig(inputConfigIndex);
    if (inputConfig == 0)
    {
        PA_LOGTHROW(MXFWriterException, ("Unknown recorder config track %d", inputConfigIndex));
    }
    
    
    // create a MXFTrackWriter for each connected and non-masked input track
    vector<RecorderInputTrackConfig*>::const_iterator iterTrackConfigs;
    for (iterTrackConfigs = inputConfig->trackConfigs.begin();
        iterTrackConfigs != inputConfig->trackConfigs.end(); 
        iterTrackConfigs++)
    {
        RecorderInputTrackConfig* trackConfig = *iterTrackConfigs;

        if (trackConfig->index < 1 || trackConfig->index > 32)
        {
            PA_LOGTHROW(MXFWriterException, ("Track config track ID %u is out of range", trackConfig->index));
        }
        
        if ((inputMask & (0x01 << (trackConfig->index - 1))) && trackConfig->isConnectedToSource())
        {
            // check the source config associated with the track has been initialised with a source package            
            if (trackConfig->sourceConfig->getSourcePackage() == 0)
            {
                PA_LOGTHROW(MXFWriterException, ("Source config associated with recorder input %d, track %d, "
                    "has not being initialised with a source package", inputConfigIndex, trackConfig->index));
            }
            
            // create a track writer        
            MXFTrackWriter* trackWriter = new MXFTrackWriter();
            _trackWriters.insert(pair<uint32_t, MXFTrackWriter*>(trackConfig->index, trackWriter));

            // get or create the output package associated with the source package for the track
            OutputPackage* outputPackage = getOutputPackage(trackConfig->sourceConfig->getSourcePackage());
            if (outputPackage == 0)
            {
                outputPackage = new OutputPackage();
                _outputPackages.push_back(outputPackage);

                outputPackage->ownPackages = true;                
                outputPackage->sourcePackage = trackConfig->sourceConfig->getSourcePackage();

                // create the material package
                outputPackage->materialPackage = new MaterialPackage();
                trackCounts.insert(pair<MaterialPackage*, TrackCount>(outputPackage->materialPackage, startTrackCount));
                outputPackage->materialPackage->uid = generateUMID();
                outputPackage->materialPackage->name = outputPackage->sourcePackage->name;
                outputPackage->materialPackage->creationDate = now;
                outputPackage->materialPackage->avidProjectName = avidProjectName;
            }
            trackWriter->outputPackage = outputPackage;
            
            // get the source package track
            prodauto::Track* sourceTrack = outputPackage->sourcePackage->getTrack(trackConfig->sourceTrackID);
            if (sourceTrack == 0)
            {
                PA_LOGTHROW(MXFWriterException, ("Failed to get source package track associated with recorder input track"));
            }

            
            // create file package associated with the source package track
            SourcePackage* filePackage = new SourcePackage();
            outputPackage->filePackages.push_back(filePackage);
            filePackage->uid = generateUMID();
            filePackage->creationDate = now;
            filePackage->avidProjectName = avidProjectName;
            filePackage->sourceConfigName = trackConfig->sourceConfig->name;
            FileEssenceDescriptor* fileDesc = new FileEssenceDescriptor();
            filePackage->descriptor = fileDesc;
            filename = createFilename(filenamePrefix, sourceTrack->dataDef == PICTURE_DATA_DEFINITION, 
                sourceTrack->id, trackConfig->index);
            _filenames.insert(pair<UMID, string>(filePackage->uid, filename));
            _filenames2.insert(pair<uint32_t, string>(trackConfig->index, filename));
            fileDesc->fileLocation = createCompleteFilename(creatingFilePath, filename);
            fileDesc->fileFormat = MXF_FILE_FORMAT_TYPE;
            if (sourceTrack->dataDef == PICTURE_DATA_DEFINITION)
            {
                fileDesc->videoResolutionID = resolutionID;
                fileDesc->imageAspectRatio = imageAspectRatio;
            }
            else
            {
                fileDesc->audioQuantizationBits = audioQuantizationBits;
            }
            
            prodauto::Track* fileTrack = new Track();
            filePackage->tracks.push_back(fileTrack);
            fileTrack->id = 1;
            fileTrack->dataDef = sourceTrack->dataDef;
            if (sourceTrack->dataDef == PICTURE_DATA_DEFINITION)
            {
                fileTrack->name = "V1";
                fileTrack->editRate = g_palEditRate;
            }
            else
            {
                fileTrack->name = "A1";
                fileTrack->editRate = g_audioEditRate;
            }
            fileTrack->sourceClip = new SourceClip();
            fileTrack->sourceClip->sourcePackageUID = outputPackage->sourcePackage->uid;
            fileTrack->sourceClip->sourceTrackID = sourceTrack->id;
            // fileTrack->length is filled in when writing is completed
            fileTrack->sourceClip->length = 0;
            if (fileTrack->editRate == g_palEditRate)
            {
                fileTrack->sourceClip->position = startPosition;
            }
            else
            {
                double factor = fileTrack->editRate.numerator * g_palEditRate.denominator /
                    (double)(fileTrack->editRate.denominator * g_palEditRate.numerator);
                fileTrack->sourceClip->position = (int64_t)(startPosition * factor + 0.5);
            }
            
            
            // create material package track associated with the file package
            TrackCount& trackCount = (*trackCounts.find(outputPackage->materialPackage)).second;
            stringstream trackName;
            if (sourceTrack->dataDef == PICTURE_DATA_DEFINITION)
            {
                if (trackConfig->number != 0)
                {
                    trackName << "V" << trackConfig->number;
                    if (trackConfig->number > trackCount.pictureTrackCount)
                    {
                        trackCount.pictureTrackCount = trackConfig->number;
                    }
                }
                else
                {
                    trackName << "V" << ++trackCount.pictureTrackCount;
                }
            }
            else
            {
                if (trackConfig->number != 0)
                {
                    trackName << "A" << trackConfig->number;
                    if (trackConfig->number > trackCount.soundTrackCount)
                    {
                        trackCount.soundTrackCount = trackConfig->number;
                    }
                }
                else
                {
                    trackName << "A" << ++trackCount.soundTrackCount;
                }
            }
            prodauto::Track* materialTrack = new Track();
            outputPackage->materialPackage->tracks.push_back(materialTrack);
            materialTrack->id = outputPackage->materialPackage->tracks.size();
            materialTrack->number = trackConfig->number;
            materialTrack->name = trackName.str();
            materialTrack->editRate = sourceTrack->editRate;
            materialTrack->dataDef = sourceTrack->dataDef;
            if (sourceTrack->dataDef == PICTURE_DATA_DEFINITION)
            {
                materialTrack->editRate = g_palEditRate;
            }
            else
            {
                materialTrack->editRate = g_audioEditRate;
            }
            materialTrack->sourceClip = new SourceClip();
            materialTrack->sourceClip->sourcePackageUID = filePackage->uid;
            materialTrack->sourceClip->sourceTrackID = fileTrack->id;
            // materialTrack->length is filled in when writing is completed
            materialTrack->sourceClip->length = 0;
            materialTrack->sourceClip->position = 0;
            
            
            // record map input track ID to material package track ID
            trackWriter->inputToMPTrackMap.insert(pair<uint32_t, uint32_t>(trackConfig->index, materialTrack->id));
            
            
            // set the status to not (yet) successfull
            _filePackageToInputTrackMap.insert(pair<UMID, uint32_t>(filePackage->uid, trackConfig->index));
            _status.insert(pair<uint32_t, bool>(trackConfig->index, false));
        }
    }
    
    // create a clip writer for each output package
    vector<OutputPackage*>::const_iterator iterOutPackages;
    for (iterOutPackages = _outputPackages.begin(); iterOutPackages != _outputPackages.end(); iterOutPackages++)
    {
        OutputPackage* outputPackage = *iterOutPackages;
        
        // create the clip writer        
        PackageDefinitions* packageDefinitions = NULL;
        outputPackage->clipWriter = NULL;
        try
        {
            mxfUMID umid;
            mxfTimestamp timestamp;
            mxfRational editRate;
            
            // map to MXF package definitions        
            
            CHECK_SUCCESS(create_package_definitions(&packageDefinitions));
            
            // map the source package
            convertUMID(outputPackage->sourcePackage->uid, umid);
            convertTimestamp(outputPackage->sourcePackage->creationDate, timestamp);
            CHECK_SUCCESS(create_tape_source_package(packageDefinitions, &umid, 
                outputPackage->sourcePackage->name.c_str(), &timestamp));
                
            // map the source package tracks
            vector<prodauto::Track*>::const_iterator iterTrack;
            for (iterTrack = outputPackage->sourcePackage->tracks.begin();
                iterTrack != outputPackage->sourcePackage->tracks.end();
                iterTrack++)
            {
                prodauto::Track* track = *iterTrack;

                ::Track* mxfTrack;

                convertUMID(track->sourceClip->sourcePackageUID, umid);
                convertRational(track->editRate, editRate);
                CHECK_SUCCESS(create_track(packageDefinitions->tapeSourcePackage, track->id, 
                    track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, track->sourceClip->sourceTrackID, 
                    track->sourceClip->position, track->sourceClip->length,
                    &mxfTrack));
            }
                
            
            // map the material package
            convertUMID(outputPackage->materialPackage->uid, umid);
            convertTimestamp(outputPackage->materialPackage->creationDate, timestamp);
            CHECK_SUCCESS(create_material_package(packageDefinitions, &umid, 
                outputPackage->materialPackage->name.c_str(), &timestamp));

            // map the material package tracks
            for (iterTrack = outputPackage->materialPackage->tracks.begin();
                iterTrack != outputPackage->materialPackage->tracks.end();
                iterTrack++)
            {
                prodauto::Track* track = *iterTrack;

                ::Track* mxfTrack;

                convertUMID(track->sourceClip->sourcePackageUID, umid);
                convertRational(track->editRate, editRate);
                CHECK_SUCCESS(create_track(packageDefinitions->materialPackage, track->id, 
                    track->number, track->name.c_str(), track->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, track->sourceClip->sourceTrackID, 
                    track->sourceClip->position, track->sourceClip->length,
                    &mxfTrack));
            }
                
            // add the user comments to the material package
            vector<UserComment>::const_iterator iterUC;
            for (iterUC = userComments.begin(); iterUC != userComments.end(); iterUC++)
            {
                const UserComment& userComment = *iterUC;
                if (userComment.name.size() == 0)
                {
                    PA_LOGTHROW(MXFWriterException, ("Invalid zero length user comment name"));
                }
                
                // add comment to material package destined for the mxf file
                CHECK_SUCCESS(add_user_comment(packageDefinitions->materialPackage, 
                    userComment.name.c_str(), userComment.value.c_str()));

                // add comment to material package destined for the database
                outputPackage->materialPackage->addUserComment(userComment.name, userComment.value);
            }
            
            
            // map the file packages
            vector<SourcePackage*>::const_iterator iterFilePackages;
            for (iterFilePackages = outputPackage->filePackages.begin(); 
                iterFilePackages != outputPackage->filePackages.end(); 
                iterFilePackages++)
            {
                SourcePackage* filePackage = *iterFilePackages;
                
                assert(filePackage->tracks.size() == 1);
                prodauto::Track* fileTrack = filePackage->tracks.back();

                // get the material package track referencing this file package
                uint32_t materialTrackID = 0;
                for (iterTrack = outputPackage->materialPackage->tracks.begin();
                    iterTrack != outputPackage->materialPackage->tracks.end();
                    iterTrack++)
                {
                    prodauto::Track* track = *iterTrack;
    
                    if (track->sourceClip->sourceTrackID == fileTrack->id)
                    {
                        materialTrackID = track->sourceClip->sourceTrackID;
                        break;
                    }
                }
                if (materialTrackID == 0)
                {
                    PA_LOGTHROW(MXFWriterException, ("Failed to find material package track referencing the file package track"));
                }

                // map the file package
                ::Package* mxfFilePackage;
                convertUMID(filePackage->uid, umid);
                convertTimestamp(filePackage->creationDate, timestamp);
                FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
                assert(fileDesc != 0);
                if (fileTrack->dataDef == PICTURE_DATA_DEFINITION)
                {
                    switch (resolutionID)
                    {
                        case UNC_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                UncUYVY, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case DV25_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                DVBased25, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case DV50_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                DVBased50, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG21_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res21;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG31_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res31;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG101_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res101;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG151S_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res151s;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG201_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res201;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case MJPEG101M_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            essenceInfo.avidMJPEGInfo.resolution = Res101m;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                AvidMJPEG, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX30_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX30, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX40_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX40, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        case IMX50_MATERIAL_RESOLUTION:
                        {
                            EssenceInfo essenceInfo;
                            CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, 
                                filePackage->name.c_str(), 
                                &timestamp, fileDesc->fileLocation.c_str(), 
                                IMX50, &essenceInfo, &mxfFilePackage));
                        }
                        break;
                        
                        default:
                            PA_LOGTHROW(MXFWriterException, ("Unsupported resolution %d", resolutionID));
                            break;
                    }
                }
                else
                {
                    EssenceInfo essenceInfo;
                    essenceInfo.pcmInfo.bitsPerSample = audioQuantizationBits;
                    CHECK_SUCCESS(create_file_source_package(packageDefinitions, &umid, filePackage->name.c_str(), 
                        &timestamp, fileDesc->fileLocation.c_str(), 
                        PCM, &essenceInfo, &mxfFilePackage));
                }
        
                // map the file package track
                ::Track* mxfTrack;
                convertUMID(fileTrack->sourceClip->sourcePackageUID, umid);
                convertRational(fileTrack->editRate, editRate);
                CHECK_SUCCESS(create_track(mxfFilePackage, fileTrack->id, 
                    0 /* number is ignored */, fileTrack->name.c_str(), 
                    fileTrack->dataDef == PICTURE_DATA_DEFINITION,
                    &editRate, &umid, fileTrack->sourceClip->sourceTrackID, 
                    fileTrack->sourceClip->position, fileTrack->sourceClip->length,
                    &mxfTrack));
            }
            
    
            // create the clip writer
            // TODO: don't hard code PAL_25i and drop frame flag
            mxfRational mxfAspectRatio;
            convertRational(imageAspectRatio, mxfAspectRatio); 
            if (!create_clip_writer(avidProjectName.size() > 0 ? avidProjectName.c_str() : 0, 
                PAL_25i, mxfAspectRatio, 0 , 1, 
                packageDefinitions, &outputPackage->clipWriter))
            {
                PA_LOGTHROW(MXFWriterException, ("Failed to create clip writer"));
            }
            
            free_package_definitions(&packageDefinitions);
        }
        catch (...)
        {
            free_package_definitions(&packageDefinitions);
            if (outputPackage->clipWriter != NULL)
            {
                abort_writing(&outputPackage->clipWriter, 1);
            }
            throw;
        }

        
        // set the clip writer in each input track writer that is part of the output package
        map<uint32_t, MXFTrackWriter*>::const_iterator iterWriters;
        for (iterWriters = _trackWriters.begin(); iterWriters != _trackWriters.end(); iterWriters++)
        {
            MXFTrackWriter* trackWriter = (*iterWriters).second;
            
            if (trackWriter->outputPackage == outputPackage)
            {
                trackWriter->clipWriter = outputPackage->clipWriter;
            }
        }
    }
}

MXFWriter::~MXFWriter()
{
    map<uint32_t, MXFTrackWriter*>::const_iterator iter1;
    for (iter1 = _trackWriters.begin(); iter1 != _trackWriters.end(); iter1++)
    {
        delete (*iter1).second;
    }

    vector<OutputPackage*>::const_iterator iter2;
    for (iter2 = _outputPackages.begin(); iter2 != _outputPackages.end(); iter2++)
    {
        delete *iter2;
    }
}

bool MXFWriter::trackIsPresent(uint32_t trackID)
{
    return _trackWriters.find(trackID) != _trackWriters.end();
}

void MXFWriter::writeSample(uint32_t trackID, uint32_t numSamples, uint8_t* data, uint32_t size)
{
    if (_isComplete)
    {
        PA_LOGTHROW(MXFWriterException, ("Recording has already completed or aborted"));
    }
    
    // check writing sample data has not started
    if (_sampleDataTracks.find(trackID) != _sampleDataTracks.end())
    {
        throw MXFWriterException("cannot write sample(s) in one go when writing sample data has started "
            "for track id %u", trackID);
    }
    
    // get the recorder input track writer
    MXFTrackWriter* trackWriter;
    map<uint32_t, MXFTrackWriter*>::iterator iter1 = _trackWriters.find(trackID);
    if (iter1 == _trackWriters.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    trackWriter = (*iter1).second;
    
    // get the corresponding material package id
    uint32_t materialTrackID;
    map<uint32_t, uint32_t>::iterator iter2 = trackWriter->inputToMPTrackMap.find(trackID);
    if (iter2 == trackWriter->inputToMPTrackMap.end())
    {
        // we shouldn't be here
        PA_LOGTHROW(MXFWriterException, ("Missing recorder input track map for id %u", trackID));
    }
    materialTrackID = (*iter2).second;
    
    // write the essence sample
    if (!write_samples(trackWriter->clipWriter, materialTrackID, numSamples, data, size))
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to write samples for recorder track %u", trackID));
    }
    
    // update the duration in the material and file packages
    trackWriter->outputPackage->updateDuration(materialTrackID, numSamples);
}

void MXFWriter::startSampleData(uint32_t trackID)
{
    if (_isComplete)
    {
        PA_LOGTHROW(MXFWriterException, ("Recording has already completed or aborted"));
    }

    // record the track id and check it wasn't already started
    if (!_sampleDataTracks.insert(trackID).second)
    {
        PA_LOGTHROW(MXFWriterException, ("startSampleData() already called for track id %u", trackID));
    }

    // get the recorder input track writer
    MXFTrackWriter* trackWriter;
    map<uint32_t, MXFTrackWriter*>::iterator iter1 = _trackWriters.find(trackID);
    if (iter1 == _trackWriters.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    trackWriter = (*iter1).second;
    
    // get the corresponding material package id
    uint32_t materialTrackID;
    map<uint32_t, uint32_t>::iterator iter2 = trackWriter->inputToMPTrackMap.find(trackID);
    if (iter2 == trackWriter->inputToMPTrackMap.end())
    {
        // we shouldn't be here
        PA_LOGTHROW(MXFWriterException, ("Missing recorder input track map for id %u", trackID));
    }
    materialTrackID = (*iter2).second;
    
    
    // start the sample data writing 
    if (!start_write_samples(trackWriter->clipWriter, materialTrackID))
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to start writing sample data for recorder track %u", trackID));
    }
}

void MXFWriter::writeSampleData(uint32_t trackID, uint8_t* data, uint32_t size)
{
    if (_isComplete)
    {
        PA_LOGTHROW(MXFWriterException, ("Recording has already completed or aborted"));
    }
    
    // check writing sample data has started
    if (_sampleDataTracks.find(trackID) == _sampleDataTracks.end())
    {
        PA_LOGTHROW(MXFWriterException, ("cannot write sample data before starting for track id %u", trackID));
    }
    
    // get the recorder input track writer
    MXFTrackWriter* trackWriter;
    map<uint32_t, MXFTrackWriter*>::iterator iter1 = _trackWriters.find(trackID);
    if (iter1 == _trackWriters.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    trackWriter = (*iter1).second;
    
    // get the corresponding material package id
    uint32_t materialTrackID;
    map<uint32_t, uint32_t>::iterator iter2 = trackWriter->inputToMPTrackMap.find(trackID);
    if (iter2 == trackWriter->inputToMPTrackMap.end())
    {
        // we shouldn't be here
        PA_LOGTHROW(MXFWriterException, ("Missing recorder input track map for id %u", trackID));
    }
    materialTrackID = (*iter2).second;
    
    // write the essence sample data
    if (!write_sample_data(trackWriter->clipWriter, materialTrackID, data, size))
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to write sample data for recorder track %u", trackID));
    }
}

void MXFWriter::endSampleData(uint32_t trackID, uint32_t numSamples)
{
    if (_isComplete)
    {
        PA_LOGTHROW(MXFWriterException, ("Recording has already completed or aborted"));
    }
    
    // check writing sample data has started and erase the record
    if (_sampleDataTracks.erase(trackID) != 1)
    {
        PA_LOGTHROW(MXFWriterException, ("cannot end writing sample data before starting for track id %u", trackID));
    }
    
    // get the recorder input track writer
    MXFTrackWriter* trackWriter;
    map<uint32_t, MXFTrackWriter*>::iterator iter1 = _trackWriters.find(trackID);
    if (iter1 == _trackWriters.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    trackWriter = (*iter1).second;
    
    // get the corresponding material package id
    uint32_t materialTrackID;
    map<uint32_t, uint32_t>::iterator iter2 = trackWriter->inputToMPTrackMap.find(trackID);
    if (iter2 == trackWriter->inputToMPTrackMap.end())
    {
        // we shouldn't be here
        PA_LOGTHROW(MXFWriterException, ("Missing recorder input track map for id %u", trackID));
    }
    materialTrackID = (*iter2).second;
    
    // complete writing the essence sample data
    if (!end_write_samples(trackWriter->clipWriter, materialTrackID, numSamples))
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to complete writing sample data for recorder track %u", trackID));
    }

    // update the duration in the material and file packages
    trackWriter->outputPackage->updateDuration(materialTrackID, numSamples);
}

void MXFWriter::completeAndSaveToDatabase()
{
    if (_isComplete)
    {
        PA_LOGTHROW(MXFWriterException, ("Recording already completed"));
    }
    // if this function is called then it is complete (even if something fails later)
    _isComplete = true;
    

    // check all sample data writing has ended
    if (_sampleDataTracks.size() > 0)
    {
        throw MXFWriterException("Cannot complete when writing because %d tracks are still writing sample data", 
            _sampleDataTracks.size());
    }

    // save the material and file packages to the database
    // save all clips individually
    int numFailed = 0;
    Database* database = Database::getInstance();
    vector<OutputPackage*>::const_iterator iter1;
    for (iter1 = _outputPackages.begin(); iter1 != _outputPackages.end(); iter1++)
    {
        OutputPackage* outputPackage = *iter1;

        // complete writing the clip
        if (!complete_writing(&outputPackage->clipWriter))
        {
            // abort writing
            abort_writing(&outputPackage->clipWriter, 0);

            // move files to the failures directory
            vector<SourcePackage*>::const_iterator iter2;
            for (iter2 = outputPackage->filePackages.begin(); iter2 != outputPackage->filePackages.end(); iter2++)
            {
                SourcePackage* filePackage = *iter2;

                string filename = (*_filenames.find(filePackage->uid)).second;
                string failureFilename = createCompleteFilename(_failuresFilePath, filename);
                
                FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
                
                try
                {
                    moveFile(fileDesc->fileLocation, failureFilename);
                }
                catch (...)
                {
                    Logging::error("Failed to move aborted (failed to complete) MXF file '%s' to failures '%s'\n",
                        fileDesc->fileLocation.c_str(), failureFilename.c_str());
                    // no rethrow, need to try move other files and complete the other output packages
                }
            }

            numFailed++;
        }
        else
        {
            bool failed = false;
            
            // move files to destination directory and update the file locations
            vector<SourcePackage*>::const_iterator iter2;
            for (iter2 = outputPackage->filePackages.begin(); iter2 != outputPackage->filePackages.end(); iter2++)
            {
                SourcePackage* filePackage = *iter2;

                string filename = (*_filenames.find(filePackage->uid)).second;
                string destFilename = createCompleteFilename(_destinationFilePath, filename);
                
                FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
                
                try
                {
                    moveFile(fileDesc->fileLocation, destFilename);
                }
                catch (...)
                {
                    Logging::error("Failed to move completed MXF file '%s' to destination '%s'\n",
                        fileDesc->fileLocation.c_str(), destFilename.c_str());
                    failed = true;
                    break;
                    // no rethrow, need to move files to the failure directory and
                    // try complete the other output packages
                }

                fileDesc->fileLocation = destFilename;
            }
            
            if (failed)
            {
                // move remaining files to failures directory
                vector<SourcePackage*>::const_iterator iter3;
                for (iter3 = iter2; iter3 != outputPackage->filePackages.end(); iter3++)
                {
                    SourcePackage* filePackage = *iter3;
    
                    string filename = (*_filenames.find(filePackage->uid)).second;
                    string failureFilename = createCompleteFilename(_failuresFilePath, filename);
                    
                    FileEssenceDescriptor* fileDesc = dynamic_cast<FileEssenceDescriptor*>(filePackage->descriptor);
                    
                    try
                    {
                        moveFile(fileDesc->fileLocation, failureFilename);
                    }
                    catch (...)
                    {
                        Logging::error("Failed to move failed MXF file '%s' to failures '%s'\n",
                            fileDesc->fileLocation.c_str(), failureFilename.c_str());
                        // no rethrow, need to move files to the failure directory and
                        // try complete the other output packages
                    }
                }
                // move ok files from destination to failure directory
                for (iter3 = outputPackage->filePackages.begin(); iter3 != iter2; iter3++)
                {
                    SourcePackage* filePackage = *iter3;
    
                    string filename = (*_filenames.find(filePackage->uid)).second;
                    string failureFilename = createCompleteFilename(_failuresFilePath, filename);
                    string destFilename = createCompleteFilename(_destinationFilePath, filename);
                    
                    try
                    {
                        moveFile(destFilename, failureFilename);
                    }
                    catch (...)
                    {
                        Logging::error("Failed to move ok MXF file '%s' to failures '%s'\n",
                            destFilename.c_str(), failureFilename.c_str());
                        // no rethrow, need to move files to the failure directory and
                        // try complete the other output packages
                    }
                }

                numFailed++;
                continue; // try the next output package
            }

            
            // save the packages to the database
            try
            {
                auto_ptr<Transaction> transaction(database->getTransaction());
            
                // save the file source packages first because material package has foreign key referencing it
                for (iter2 = outputPackage->filePackages.begin(); iter2 != outputPackage->filePackages.end(); iter2++)
                {
                    SourcePackage* filePackage = *iter2;
        
                    database->savePackage(filePackage, transaction.get());
                }
        
                database->savePackage(outputPackage->materialPackage, transaction.get());
        
                transaction->commitTransaction();

                // record the successes
                for (iter2 = outputPackage->filePackages.begin(); iter2 != outputPackage->filePackages.end(); iter2++)
                {
                    SourcePackage* filePackage = *iter2;
                    
                    // set the status to successfull
                    map<UMID, uint32_t>::iterator iter3 = _filePackageToInputTrackMap.find(filePackage->uid);
                    if (iter3 == _filePackageToInputTrackMap.end())
                    {
                        PA_LOGTHROW(MXFWriterException, ("Bug: could not map file package UMID to input track"));
                    }
                    _status.erase((*iter3).second);
                    _status.insert(pair<uint32_t, bool>((*iter3).second, true));
                }
            }
            catch (...)
            {
                Logging::error("Failed to save packages to database\n");

                // move ok files from destination to failure directory
                for (iter2 = outputPackage->filePackages.begin(); iter2 != outputPackage->filePackages.end(); iter2++)
                {
                    SourcePackage* filePackage = *iter2;
    
                    string filename = (*_filenames.find(filePackage->uid)).second;
                    string failureFilename = createCompleteFilename(_failuresFilePath, filename);
                    string destFilename = createCompleteFilename(_destinationFilePath, filename);
                    
                    try
                    {
                        moveFile(destFilename, failureFilename);
                    }
                    catch (...)
                    {
                        Logging::error("Failed to move ok MXF file '%s' to failures '%s'\n",
                            destFilename.c_str(), failureFilename.c_str());
                        // no rethrow, need to move files to the failure directory and
                        // try complete the other output packages
                    }
                }

                numFailed++;
                // no rethrow, need to try complete the other output packages
            }
        }
    }
    
    if (numFailed > 0)
    {
        PA_LOGTHROW(MXFWriterException, ("Failed to complete and save %d clips", numFailed));
    }

}

OutputPackage* MXFWriter::getOutputPackage(SourcePackage* sourcePackage)
{
    vector<OutputPackage*>::const_iterator iter;
    for (iter = _outputPackages.begin(); iter != _outputPackages.end(); iter++)
    {
        if ((*iter)->sourcePackage == sourcePackage)
        {
            return *iter;
        }
    }
    
    return 0;
}

SourcePackage* MXFWriter::getSourcePackage(vector<SourcePackage*>& sourcePackages, 
    UMID sourcePackageUID, uint32_t sourceTrackID)

{
    SourcePackage* sourcePackage;
    
    vector<SourcePackage*>::const_iterator iter;
    for (iter = sourcePackages.begin(); iter != sourcePackages.end(); iter++)
    {
        sourcePackage = *iter;
        
        if (sourcePackage->uid == sourcePackageUID)
        {
            if (sourcePackage->getTrack(sourceTrackID))
            {
                return sourcePackage;
            }
            else
            {
                PA_LOG(warning, ("Source package matched but track not found"));
                return 0;
            }
        }
    }
    return 0;
}

bool MXFWriter::wasSuccessfull(uint32_t trackID)
{
    map<uint32_t, bool>::iterator iter = _status.find(trackID);
    if (iter == _status.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    return (*iter).second;
}

string MXFWriter::getFilename(uint32_t trackID)
{
    map<uint32_t, string>::iterator iter = _filenames2.find(trackID);
    if (iter == _filenames2.end())
    {
        PA_LOGTHROW(MXFWriterException, ("Invalid recorder input track id %u", trackID));
    }
    return (*iter).second;
}

string MXFWriter::getCreatingFilename(string filename)
{
    return createCompleteFilename(_creatingFilePath, filename);
}

string MXFWriter::getDestinationFilename(string filename)
{
    return createCompleteFilename(_destinationFilePath, filename);
}

string MXFWriter::getFailuresFilename(string filename)
{
    return createCompleteFilename(_failuresFilePath, filename);
}
    
