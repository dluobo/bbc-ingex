/*
 * $Id: create_aaf.cpp,v 1.8 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * Creates AAF files with clips extracted from the database
 *
 * Copyright (C) 2008, BBC, Nicholas Pinks <npinks@users.sourceforge.net>
 * Copyright (C) 2008, BBC, Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

#include "AAFFile.h"
#include "FCPFile.h"
#include "CreateAAFException.h"
#include "CutsDatabase.h"

#include <Database.h>
#include <MCClipDef.h>
#include <Utilities.h>
#include <DBException.h>
#include <Timecode.h>
#include <XmlTools.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOM.hpp>


using namespace std;
using namespace prodauto;


static const char* g_dns = "prodautodb";
static const char* g_databaseUserName = "bamzooki";
static const char* g_databasePassword = "bamzooki";
static const char* g_filenamePrefix = "ingex";
static const char* g_resultsPrefix = "RESULTS:";

// utility class to clean-up Package pointers
class MaterialHolder
{
public:
    ~MaterialHolder()
    {
        PackageSet::iterator iter;
        for (iter = packages.begin(); iter != packages.end(); iter++)
        {
            delete *iter;
        }
        
        // topPackages only holds references so don't delete
    }
    
    MaterialPackageSet topPackages; 
    PackageSet packages;
};   

static void parseDateAndTimecode(string dateAndTimecodeStr, bool isPAL, Date* date, int64_t* timecode)
{
    int data[7];
    int timecodeBase = (isPAL ? 25 : 30);
    
    if (sscanf(dateAndTimecodeStr.c_str(), "%u-%u-%uS%u:%u:%u:%u", &data[0], &data[1],
        &data[2], &data[3], &data[4], &data[5], &data[6]) != 7)
    {
        throw "Invalid date and timecode string";
    }
    
    date->year = data[0];
    date->month = data[1];
    date->day = data[2];
    *timecode = data[3] * 60 * 60 * timecodeBase +
        data[4] * 60 * timecodeBase + 
        data[5] * timecodeBase + 
        data[6];
}

static void parseTimestamp(string timestampStr, Timestamp* ts)
{
    int data[6];
    
    if (sscanf(timestampStr.c_str(), "%u-%u-%uT%u:%u:%u", &data[0], &data[1],
        &data[2], &data[3], &data[4], &data[5]) != 6)
    {
        throw "Invalid timestamp string";
    }
    
    ts->year = data[0];
    ts->month = data[1];
    ts->day = data[2];
    ts->hour = data[3];
    ts->min = data[4];
    ts->sec = data[5];
}

static void parseTag(string tag, string& tagName, string& tagValue)
{
    size_t index;
    if ((index = tag.find("=")) != string::npos && index + 1 != tag.size())
    {
        tagName = tag.substr(0, index);
        tagValue = tag.substr(index + 1, tag.size() - index);
    }
    else
    {
        throw "Failed to parse tag";
    }
}


static string dateAndTimecodeString(Date date, int64_t timecode, bool isPAL)
{
    char dateAndTimecodeStr[48];
    int timecodeBase = (isPAL ? 25 : 30);
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        dateAndTimecodeStr, 48, "%04d%02u%02u%02u%02u%02u%02u",
        date.year, date.month, date.day, 
        (int)(timecode / (60 * 60 * timecodeBase)), 
        (int)((timecode % (60 * 60 * timecodeBase)) / (60 * timecodeBase)),
        (int)(((timecode % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) / timecodeBase),
        (int)(((timecode % (60 * 60 * timecodeBase)) % (60 * timecodeBase)) % timecodeBase));
        
    return dateAndTimecodeStr;
}

static string timestampString(Timestamp timestamp)
{
    char timestampStr[22];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        timestampStr, 22, "%04d%02u%02u%02u%02u%02u",
        timestamp.year, timestamp.month, timestamp.day, 
        timestamp.hour, timestamp.min, timestamp.sec);
        
    return timestampStr;
}

static string createDateAndTimecodeSuffix(Date fromDate, int64_t fromTimecode, Date toDate, int64_t toTimecode, bool isPAL)
{
    stringstream suffix;
    
    suffix << dateAndTimecodeString(fromDate, fromTimecode, isPAL) << "_" << dateAndTimecodeString(toDate, toTimecode, isPAL);
        
    return suffix.str();
}

static string createTagSuffix()
{
    return timestampString(generateTimestampNow());
}

static string createSingleClipFilename(string prefix, string suffix, int index)
{
    stringstream filename;
    
    filename << prefix << "_" << suffix << "_" << index << ".aaf";
        
    return filename.str();
}

static string createXMLFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_" << suffix << "_" <<".xml";
        
    return filename.str();
}

static string createMCClipFilename(string prefix, int64_t startTime, string suffix, int index)
{
    stringstream filename;
    
    filename << prefix << "_mc_" << startTime << "_"  << suffix << "_" << index << ".aaf";
        
    return filename.str();
}

static string createAAFGroupSingleFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_g_" << suffix << ".aaf";
        
    return filename.str();
}

static string createAAFGroupMCFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_g_mc_" << suffix << ".aaf";
        
    return filename.str();
}

static string createUniqueFilename(string prefix)
{
    string result;
    int count = 0;
    struct stat statBuf;
    
    while (count < 10000) // more than 10000 files with the same name - you must be joking!
    {
        stringstream filename;
        
        if (count == 0)
        {
            filename << prefix << ".aaf";
        }
        else
        {
            filename << prefix << "_" << count << ".aaf";
        }
        
        if (stat(filename.str().c_str(), &statBuf) != 0)
        {
            // file does not exist - use it
            result = filename.str();
            break;
        }
        
        count++;
    }
    if (result.size() == 0)
    {
        throw "Failed to create unique filename";
    }
    
    return result;
}

static string addFilename(vector<string>& filenames, string filename)
{
    filenames.push_back(filename);
    return filename;
}

static set<string> getSourceConfigNames(PackageSet& packages, Package* package)
{
    set<string> sources;
    
    if (package->getType() == FILE_ESSENCE_DESC_TYPE)
    {
        sources.insert(dynamic_cast<SourcePackage*>(package)->sourceConfigName);
        return sources;
    }
    
    vector<Track*>::const_iterator iter1;
    for (iter1 = package->tracks.begin(); iter1 != package->tracks.end(); iter1++)
    {
        Track* track = *iter1;

        SourcePackage dummy;
        dummy.uid = track->sourceClip->sourcePackageUID;
        PackageSet::iterator result = packages.find(&dummy);
        if (result != packages.end())
        {
            Package* referencedPackage = *result;
            
            set<string> refSources = getSourceConfigNames(packages, referencedPackage);
            sources.insert(refSources.begin(), refSources.end());
        }
    }
    
    return sources;
}

static void getDirectorsCutSources(MCClipDef* mcClipDef, MaterialPackageSet& materialPackages, PackageSet& packages,
    set<string>* sources, string* defaultSource)
{
    // get the source config names for the material packages

    set<string> packageSources;
    MaterialPackageSet::const_iterator iter1;
    for (iter1 = materialPackages.begin(); iter1 != materialPackages.end(); iter1++)
    {
        MaterialPackage* materialPackage = *iter1;
        
        set<string> sources = getSourceConfigNames(packages, materialPackage);
        packageSources.insert(sources.begin(), sources.end());
    }
    
    // intersect the material package sources with the sources for the selectable track group 
    // in the multi-camera group
    
    uint32_t defaultSourceNumber = 0xffffffff;
    map<uint32_t, MCTrackDef*>::const_iterator iter2;
    for (iter2 = mcClipDef->trackDefs.begin(); iter2 != mcClipDef->trackDefs.end(); iter2++)
    {
        MCTrackDef* trackDef = (*iter2).second;
        
        if (trackDef->selectorDefs.size() > 1)
        {
            map<uint32_t, MCSelectorDef*>::const_iterator iter3;
            for (iter3 = trackDef->selectorDefs.begin(); iter3 != trackDef->selectorDefs.end(); iter3++)
            {
                MCSelectorDef* selectorDef = (*iter3).second;
                uint32_t selectorNumber = (*iter3).first;
                
                if (selectorDef->sourceConfig != 0)
                {
                    if (packageSources.find(selectorDef->sourceConfig->name) == packageSources.end())
                    {
                        // source not referenced by the material packages
                        continue;
                    }
                    
                    if (selectorNumber < defaultSourceNumber)
                    {
                        *defaultSource = selectorDef->sourceConfig->name;
                        defaultSourceNumber = selectorNumber; 
                    }
                    sources->insert(selectorDef->sourceConfig->name);
                }
            }
            break;
        }
    }
    
    
}

static vector<CutInfo> getDirectorsCutSequence(CutsDatabase* database, MCClipDef* mcClipDef, 
    MaterialPackageSet& materialPackages, PackageSet& packages,
    Timestamp creationTs, int64_t startTimecode)
{
    set<string> sources;
    string defaultSource;
    getDirectorsCutSources(mcClipDef, materialPackages, packages, &sources, &defaultSource);

    if (sources.size() < 2)
    {
        return vector<CutInfo>();
    }
    
    Date creationDate;
    creationDate.year = creationTs.year;
    creationDate.month = creationTs.month;
    creationDate.day = creationTs.day;

    return database->getCuts(creationDate, startTimecode, sources, defaultSource);
}




static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Output logging to stdout followed by '\\nRESULTS:\\n' which signals the start of results.\n");
    fprintf(stderr, "The results is a newline separated list of the following elements:\n");
    fprintf(stderr, "    * Total clips\n");
    fprintf(stderr, "    * Total multi-camera groups\n");
    fprintf(stderr, "    * Total director cut sequences\n");
    fprintf(stderr, "    * List of output filenames separated by a newline\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help                     Display this usage message\n");
    fprintf(stderr, "  -v, --verbose                  Print status messages\n");
    fprintf(stderr, "  -p, --prefix <filepath>        Filename prefix used for output files (default '%s')\n", g_filenamePrefix);
    fprintf(stderr, "  -r --resolution <id>           Video resolution identifier as listed in the database\n"); 
    fprintf(stderr, "                                     (default is %d for DV-50)\n", DV50_MATERIAL_RESOLUTION);
    fprintf(stderr, "  -g, --group                    Create AAF file with all clips included\n");
    fprintf(stderr, "  -o, --grouponly                Only create AAF file with all clips included\n");
    fprintf(stderr, "      --no-ts-suffix             Don't use a timestamp for a group only file\n");
    fprintf(stderr, "  -m, --multicam                 Also create multi-camera clips\n");
    fprintf(stderr, "  --ntsc                         Targets NTSC sources (default is PAL)\n");
    fprintf(stderr, "  -f, --from <date>S<timecode>   Includes clips created >= date and start timecode\n");
    fprintf(stderr, "  -t, --to <date>S<timecode>     Includes clips created < date and start timecode\n");
    fprintf(stderr, "  -c, --from-cd <date>T<time>    Includes clips created >= CreationDate of clip\n");
    fprintf(stderr, "  --tag <name>=<value>           Includes clips with user comment tag <name> == <value>\n");
    fprintf(stderr, "  --mc-cuts <db name>            Includes sequence of multi-camera cuts from database\n");
    fprintf(stderr, "  --aaf-xml                      Outputs AAF file using the XML stored format\n");
    fprintf(stderr, "  --audio-edits                  Include edits in the audio tracks in the multi-camera cut sequence\n");
    fprintf(stderr, "  --fcp-xml                      Prints Apple XML for Final Cut Pro- disables AAF\n");
    fprintf(stderr, "  --fcp-path <string>            Sets the path for FCP to see Ingex generated media\n");
    fprintf(stderr, "  -d, --dns <string>             Database DNS (default '%s')\n", g_dns);
    fprintf(stderr, "  -u, --dbuser <string>          Database user name (default '%s')\n", g_databaseUserName);
    fprintf(stderr, "  --dbpassword <string>          Database user password (default ***)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "* --from and --to form is 'yyyy-mm-ddShh:mm:ss:ff' (PAL: ff ranges 1..24, NTSC: ff ranges 1..29 and is non-drop frame)\n");
    fprintf(stderr, "* --from-cd form is 'yyyy-mm-ddThh:mm:ss'\n");
    fprintf(stderr, "* The default for --from is the start of today\n"); 
    fprintf(stderr, "* The default for --to is the start of tommorrow\n");
    fprintf(stderr, "* If --tag is used then --to and --from are ignored\n");
}


int main(int argc, const char* argv[])
{
    int cmdlnIndex = 1;
    string filenamePrefix = g_filenamePrefix;
    bool createAAFGroup = false;
    bool createAAFGroupOnly = false;
    bool createMultiCam = false;
    bool aafxml = false;
    bool fcpxml = false;
    bool audioEdits = false;
    Date fromDate;
    Date toDate;
    int64_t fromTimecode = 0;
    int64_t toTimecode = 0;
    string dns = g_dns;
    string dbUserName = g_databaseUserName;
    string dbPassword = g_databasePassword;
    string tagName;
    string tagValue;
    string suffix;
    vector<string> filenames;
    int videoResolutionID = DV50_MATERIAL_RESOLUTION;
    bool verbose = false;
    bool noTSSuffix = false;
    string mcCutsFilename;
    bool includeMCCutsSequence = false;
    CutsDatabase* mcCutsDatabase = 0;
    Timestamp fromCreationDate = g_nullTimestamp;
    string fcpPath;
    bool isPAL = true;
    const char* toString = 0;
    const char* fromString = 0;
    Rational targetEditRate = g_palEditRate;
    
    Timestamp t = generateTimestampStartToday();
    fromDate.year = t.year;
    fromDate.month = t.month;
    fromDate.day = t.day;
    t = generateTimestampStartTomorrow();
    toDate.year = t.year;
    toDate.month = t.month;
    toDate.day = t.day;


    
    try
    {
        while (cmdlnIndex < argc)
        {
            if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
                strcmp(argv[cmdlnIndex], "--help") == 0)
            {
                usage(argv[0]);
                return 0;
            }
            else if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
                strcmp(argv[cmdlnIndex], "--verbose") == 0)
            {
                verbose = true;
                cmdlnIndex++;
            }
            else if (strcmp(argv[cmdlnIndex], "-p") == 0 ||
                strcmp(argv[cmdlnIndex], "--prefix") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                filenamePrefix = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-r") == 0 ||
                strcmp(argv[cmdlnIndex], "--resolution") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for --resolution\n");
                    return 1;
                }
                if (sscanf(argv[cmdlnIndex + 1], "%d\n", &videoResolutionID) != 1)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Invalid argument for --resolution\n");
                    return 1;
                }
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-g") == 0 ||
                strcmp(argv[cmdlnIndex], "--group") == 0)
            {
                createAAFGroup = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-o") == 0 ||
                strcmp(argv[cmdlnIndex], "--grouponly") == 0)
            {
                createAAFGroupOnly = true;
                createAAFGroup = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "--no-ts-suffix") == 0)
            {
                noTSSuffix = true;
                cmdlnIndex++;
            }
            else if (strcmp(argv[cmdlnIndex], "-m") == 0 ||
                strcmp(argv[cmdlnIndex], "--multicam") == 0)
            {
                createMultiCam = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "--ntsc") == 0)
            {
                isPAL = false;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-f") == 0 ||
                strcmp(argv[cmdlnIndex], "--from") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                fromString = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-t") == 0 ||
                strcmp(argv[cmdlnIndex], "--to") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                toString = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-c") == 0 ||
                strcmp(argv[cmdlnIndex], "--from-cd") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                parseTimestamp(argv[cmdlnIndex + 1], &fromCreationDate);
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--tag") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                parseTag(argv[cmdlnIndex + 1], tagName, tagValue);
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--mc-cuts") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                mcCutsFilename = argv[cmdlnIndex + 1];
                includeMCCutsSequence = true;
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-d") == 0 ||
                strcmp(argv[cmdlnIndex], "--dns") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dns = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "-u") == 0 ||
                strcmp(argv[cmdlnIndex], "--dbuser") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dbUserName = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--dbpassword") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                dbPassword = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else if (strcmp(argv[cmdlnIndex], "--audio-edits") == 0)
            {
                audioEdits = true;
                cmdlnIndex++;
            }
            else if (strcmp(argv[cmdlnIndex], "--aaf-xml") == 0)
            {
                aafxml = true;
                cmdlnIndex++;
            }
            else if (strcmp(argv[cmdlnIndex], "--fcp-xml") == 0)
            {
                fcpxml = true;
                cmdlnIndex++;
            }
            else if (strcmp(argv[cmdlnIndex], "--fcp-path") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                fcpPath = argv[cmdlnIndex + 1];
                cmdlnIndex += 2;
            }
            else
            {
                usage(argv[0]);
                fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
        }
    }
    catch (const char*& ex)
    {
        usage(argv[0]);
        fprintf(stderr, "\nFailed to parse arguments:\n%s\n", ex);
        return 1;
    }
    catch (...)
    {
        usage(argv[0]);
        fprintf(stderr, "\nFailed to parse arguments:\nUnknown exception thrown\n");
        return 1;
    }
    
    targetEditRate = (isPAL ? g_palEditRate : g_ntscEditRate);
    
    // parse --to and --from now that we know whether it is PAL or NTSC
    if (fromString != 0)
    {
        parseDateAndTimecode(fromString, isPAL, &fromDate, &fromTimecode);
    }
    if (toString != 0)
    {
        parseDateAndTimecode(toString, isPAL, &toDate, &toTimecode);
    }

    if (tagName.size() > 0)
    {
        suffix = createTagSuffix();
    }
    else
    {
        suffix = createDateAndTimecodeSuffix(fromDate, fromTimecode, toDate, toTimecode, isPAL);
    }
    
    
    // initialise the prodauto database
    try
    {
        Database::initialise(dns, dbUserName, dbPassword, 1, 3);
        if (verbose)
        {
            printf("Initialised the database connection\n");
        }
    }
    catch (DBException& ex)
    {
        fprintf(stderr, "Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
        return 1;
    }

    // open the cuts database
    if (includeMCCutsSequence)
    {
        try
        {
            mcCutsDatabase = new CutsDatabase(mcCutsFilename);
        }
        catch (const ProdAutoException& ex)
        {
            fprintf(stderr, "Failed to open cuts database '%s':\n  %s\n", mcCutsFilename.c_str(), ex.getMessage().c_str());
            return 1;
        }
    }

    // load the material    
    Database* database = Database::getInstance();
    auto_ptr<EditorsFile> editorsFile;
    MaterialHolder material;
    VectorGuard<MCClipDef> mcClipDefs;
    mcClipDefs.get() = database->loadAllMultiCameraClipDefs();
    try
    {
        if (tagName.size() != 0)
        {
            // load all material with tagged value name = tagName and value = tagValue
            database->loadMaterial(tagName.c_str(), tagValue.c_str(), &material.topPackages, &material.packages);
            if (verbose)
            {
                printf("Loaded %d clips from the database based on the tag %s=%s\n", (int)material.topPackages.size(), tagName.c_str(), tagValue.c_str());
            }
        }
        else if (fromCreationDate.year != 0)
        {
            Timestamp endDayTo = g_nullTimestamp;
            endDayTo.year = fromCreationDate.year;
            endDayTo.month = fromCreationDate.month;
            endDayTo.day = fromCreationDate.day;
            endDayTo.hour = 23;
            endDayTo.min = 59;
            endDayTo.sec = 59;
            endDayTo.qmsec = 249;
            
            // load all material created >= fromCreationDate until the end of the day
            database->loadMaterial(fromCreationDate, endDayTo, &material.topPackages, &material.packages);

            if (verbose)
            {
                printf("Loaded %d clips from the database from creation date %s\n", (int)material.topPackages.size(), timestampString(fromCreationDate).c_str());
            }
        }
        else
        {
            Timestamp startDayFrom = g_nullTimestamp;
            startDayFrom.year = fromDate.year;
            startDayFrom.month = fromDate.month;
            startDayFrom.day = fromDate.day;
            Timestamp endDayTo = g_nullTimestamp;
            endDayTo.year = toDate.year;
            endDayTo.month = toDate.month;
            endDayTo.day = toDate.day;
            endDayTo.hour = 23;
            endDayTo.min = 59;
            endDayTo.sec = 59;
            endDayTo.qmsec = 249;
            
            // load all material created >= (start of the day) from and < (end of the day) to
            database->loadMaterial(startDayFrom, endDayTo, &material.topPackages, &material.packages);

            if (verbose)
            {
                printf("Loaded %d clips from the database based on the dates alone\n", (int)material.topPackages.size());
            }


            // remove all packages that are < fromTimecode (at fromDate) and > toTimecode (at toDate)
            vector<prodauto::MaterialPackage *> packagesToErase;
            MaterialPackageSet::const_iterator it;
            for (it = material.topPackages.begin(); it != material.topPackages.end(); ++it)
            {
                MaterialPackage* topPackage = *it;
                
                if (topPackage->creationDate.year == fromDate.year && 
                    topPackage->creationDate.month == fromDate.month && 
                    topPackage->creationDate.day == fromDate.day &&
                    getStartTime(topPackage, material.packages, targetEditRate) < fromTimecode)
                {
                    packagesToErase.push_back(topPackage);
                }
                else if (topPackage->creationDate.year == toDate.year && 
                    topPackage->creationDate.month == toDate.month && 
                    topPackage->creationDate.day == toDate.day &&
                    getStartTime(topPackage, material.packages, targetEditRate) >= toTimecode)
                {
                    packagesToErase.push_back(topPackage);
                }
            }
            vector<prodauto::MaterialPackage*>::const_iterator it2;
            for (it2 = packagesToErase.begin(); it2 != packagesToErase.end(); it2++)
            {
                material.topPackages.erase(*it2);
            }

            if (verbose)
            {
                if (packagesToErase.size() > 0)
                {
                    printf("Removed %zd clips from those loaded that did not match the start timecode range\n", packagesToErase.size());
                }
                else
                {
                    printf("All clips match the start timecode range\n");
                }
            }
        }
    }
    catch (const char*& ex)
    {
        fprintf(stderr, "\nFailed to create:\n%s\n", ex);
        Database::close();
        return 1;
    }
    catch (ProdAutoException& ex)
    {
        fprintf(stderr, "\nFailed to create:\n%s\n", ex.getMessage().c_str());
        Database::close();    
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "\nFailed to create:\nUnknown exception thrown\n");
        Database::close();    
        return 1;
    }
    
    
    // remove any packages with an edit rate != the target edit rate and
    // go through the material package -> file package and remove any that reference
    // a file package with a non-zero videoResolutionID and !=  targetVideoResolutionID
    vector<prodauto::MaterialPackage *> packagesToErase;
    MaterialPackageSet::const_iterator iter1;
    for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
    {
        MaterialPackage* topPackage = *iter1;

        // check package edit rate        
        Rational packageEditRate = getVideoEditRate(topPackage, material.packages);
        if (packageEditRate != targetEditRate && packageEditRate != g_nullRational)
        {
            packagesToErase.push_back(topPackage);
            continue;
        }

        // check video resolution IDs
        vector<Track*>::const_iterator iter2;
        for (iter2 = topPackage->tracks.begin(); iter2 != topPackage->tracks.end(); iter2++)
        {
            Track* track = *iter2;
            
            SourcePackage dummy;
            dummy.uid = track->sourceClip->sourcePackageUID;
            PackageSet::iterator result = material.packages.find(&dummy);
            if (result != material.packages.end())
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
                
                FileEssenceDescriptor* fileDescriptor = dynamic_cast<FileEssenceDescriptor*>(
                    sourcePackage->descriptor);
                if (fileDescriptor->videoResolutionID != 0 && 
                    fileDescriptor->videoResolutionID != videoResolutionID)
                {
                    // material package has wrong video resolution
                    packagesToErase.push_back(topPackage);
                    break; // break out of track loop
                }
            }

        }
    }
    vector<prodauto::MaterialPackage *>::const_iterator it;
    for (it = packagesToErase.begin(); it != packagesToErase.end(); it++)
    {
        material.topPackages.erase(*it);
    }
    
    if (verbose)
    {
        if (packagesToErase.size() > 0)
        {
            printf("Removed %zd clips from those loaded that did not match the video resolution\n", packagesToErase.size());
        }
        else
        {
            printf("All clips either match the video resolution or are audio only\n");
        }
    }

    // So we now have the relevant MaterialPackages in...    material.topPackages
    // and relevant SourcePackages in...                     material.packages
        
    
    int totalClips = 0;
    int totalMulticamGroups = 0;
    int totalDirectorsCutsSequences = 0;

    
    // create AAF file if there is material    
    if (material.topPackages.size() == 0)
    {
        // Results
        
        printf("\n%s\n", g_resultsPrefix);
        printf("%d\n%d\n%d\n", totalClips, totalMulticamGroups, totalDirectorsCutsSequences);
    }
    else
    {
        try
        {
            // create FCP or AAF group files
            if (fcpxml)
            {
                editorsFile = auto_ptr<EditorsFile>(new FCPFile(addFilename(filenames, createXMLFilename(filenamePrefix, suffix)), fcpPath));
            }
            else
            {
                if (createAAFGroup)
                {
                    if (createAAFGroupOnly && noTSSuffix)
                    {
                        editorsFile = auto_ptr<EditorsFile>(new AAFFile(
                            addFilename(filenames, createUniqueFilename(filenamePrefix)), targetEditRate, aafxml, audioEdits));
                    }
                    else if (createMultiCam)
                    {
                        editorsFile = auto_ptr<EditorsFile>(new AAFFile(
                            addFilename(filenames, createAAFGroupMCFilename(filenamePrefix, suffix)), targetEditRate, aafxml, audioEdits));
                    }
                    else
                    {
                        editorsFile = auto_ptr<EditorsFile>(new AAFFile(
                            addFilename(filenames, createAAFGroupSingleFilename(filenamePrefix, suffix)), targetEditRate, aafxml, audioEdits));
                    }
                }
            }

            
            // create clips
            
            MaterialPackageSet::iterator iter1;
            int index = 0;
            for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
            {
                MaterialPackage* topPackage = *iter1;
                
                if (!fcpxml && !createAAFGroupOnly)
                {
                    AAFFile aafFile(addFilename(filenames, createSingleClipFilename(filenamePrefix, suffix, index++)), targetEditRate, aafxml, audioEdits);
                    aafFile.addClip(topPackage, material.packages);
                    aafFile.save();
                }
    
                if (createAAFGroup)
                {
                    editorsFile->addClip(topPackage, material.packages);                
                }

                totalClips++;
            }
            
            
            // create multi-camera clips
            // mult-camera clips group packages together which have the same start time and creation date
            
            if (createMultiCam)
            {          
                // load multi-camera clip definitions
                MaterialPackageSet donePackages;
                
                for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
                {
                    MaterialPackage* topPackage1 = *iter1;
        
                    if (!donePackages.insert(topPackage1).second)
                    {
                        // already done this package
                        continue;
                    }
                    
                    MaterialPackageSet materialPackages;
                    materialPackages.insert(topPackage1);
                    
                    // add material to group that has same start time and creation date
                    MaterialPackageSet::iterator iter2;
                    for (iter2 = iter1, iter2++; iter2 != material.topPackages.end(); iter2++)
                    {
                        MaterialPackage* topPackage2 = *iter2;
                        
                        if (topPackage2->creationDate.year == topPackage1->creationDate.year &&
                            topPackage2->creationDate.month == topPackage1->creationDate.month &&
                            topPackage2->creationDate.day == topPackage1->creationDate.day &&
                            getStartTime(topPackage1, material.packages, targetEditRate) == 
                                getStartTime(topPackage2, material.packages, targetEditRate))
                        {
                            materialPackages.insert(topPackage2);
                            donePackages.insert(topPackage2);
                        }
                    }

                    // materialPackages now contains all MaterialPackages with same start time and creation date

                    vector<MCClipDef*>::iterator iter3;
                    int index = 0;
            
                    for (iter3 = mcClipDefs.get().begin(); iter3 != mcClipDefs.get().end(); iter3++)
                    {
                        MCClipDef* mcClipDef = *iter3;
                        vector<CutInfo> sequence;
                        
                        // exclude clip defs referencing sources with wrong video edit rate
                        Rational mcClipDefEditRate = getVideoEditRate(mcClipDef);
                        if (mcClipDefEditRate != targetEditRate && mcClipDefEditRate != g_nullRational)
                        {
                            continue;
                        }
                        
                        if (mcCutsDatabase != 0)
                        {
                            // TODO: director's cut database doesn't yet have flag indicating PAL/NTSC
                            sequence = getDirectorsCutSequence(mcCutsDatabase, mcClipDef, materialPackages, material.packages, 
                                topPackage1->creationDate, getStartTime(topPackage1, material.packages, targetEditRate));
                        }

                        if (!fcpxml && !createAAFGroupOnly)
                        {
                            AAFFile aafFile(addFilename(filenames, createMCClipFilename(filenamePrefix, getStartTime(topPackage1, material.packages, targetEditRate), suffix, index++)), targetEditRate, aafxml, audioEdits); 
                            aafFile.addMCClip(mcClipDef, materialPackages, material.packages, sequence);
                            aafFile.save();
                        }       

                        if (fcpxml || createAAFGroup)
                        {
                            if (editorsFile->addMCClip(mcClipDef, materialPackages, material.packages, sequence))
                            {
                                totalMulticamGroups++;
                            }
                        }
                        
                        totalDirectorsCutsSequences = sequence.empty() ? totalDirectorsCutsSequences : totalDirectorsCutsSequences + 1;
                        printf("Directors cut sequence size = %zd\n", sequence.size());
                    }
                } // iterate over material.topPackages
            }
            
            if (createAAFGroup)
            {
                editorsFile->save();
                delete editorsFile.release();
            }
            // Results
            
            printf("\n%s\n", g_resultsPrefix);
            printf("%d\n%d\n%d\n", totalClips, totalMulticamGroups, totalDirectorsCutsSequences);
            
            vector<string>::const_iterator filenamesIter;
            for (filenamesIter = filenames.begin(); filenamesIter != filenames.end(); filenamesIter++)
            {
                printf("%s\n", (*filenamesIter).c_str());
            }
        }
        catch (const char*& ex)
        {
            fprintf(stderr, "\nFailed to create:\n%s\n", ex);
            Database::close();
            return 1;
        }
        catch (ProdAutoException& ex)
        {
            fprintf(stderr, "\nFailed to create:\n%s\n", ex.getMessage().c_str());
            Database::close();
            return 1;
        }
        catch (...)
        {
            fprintf(stderr, "\nFailed to create:\nUnknown exception thrown\n");
            Database::close();
            return 1;
        }
    }
    Database::close();
    return 0;
}


