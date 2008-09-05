/*
 * $Id: create_aaf.cpp,v 1.4 2008/09/05 16:48:33 john_f Exp $
 *
 * Creates AAF files with clips extracted from the database
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sstream>

#include "CreateAAF.h"
#include "CreateAAFException.h"
#include "CutsDatabase.h"
#include <Database.h>
#include <MCClipDef.h>
#include <Utilities.h>
#include <DBException.h>
#include "Timecode.h"

#include "XmlTools.h"
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


static void parseDateAndTimecode(string dateAndTimecodeStr, Date* date, int64_t* timecode)
{
    int data[7];
    
    if (sscanf(dateAndTimecodeStr.c_str(), "%u-%u-%uS%u:%u:%u:%u", &data[0], &data[1],
        &data[2], &data[3], &data[4], &data[5], &data[6]) != 7)
    {
        throw "Invalid date and timecode string";
    }
    
    date->year = data[0];
    date->month = data[1];
    date->day = data[2];
    *timecode = data[3] * 60 * 60 * 25 +
        data[4] * 60 * 25 + 
        data[5] * 25 + 
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


static string dateAndTimecodeString(Date date, int64_t timecode)
{
    char dateAndTimecodeStr[48];
    
#if defined(_MSC_VER)    
    _snprintf(
#else
    snprintf(
#endif
        dateAndTimecodeStr, 48, "%04d%02u%02u%02u%02u%02u%02u",
        date.year, date.month, date.day, 
        (int)(timecode / (60 * 60 * 25)), 
        (int)((timecode % (60 * 60 * 25)) / (60 * 25)),
        (int)(((timecode % (60 * 60 * 25)) % (60 * 25)) / 25),
        (int)(((timecode % (60 * 60 * 25)) % (60 * 25)) % 25));
        
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

static string createDateAndTimecodeSuffix(Date fromDate, int64_t fromTimecode, Date toDate, int64_t toTimecode)
{
    stringstream suffix;
    
    suffix << dateAndTimecodeString(fromDate, fromTimecode) << "_" << dateAndTimecodeString(toDate, toTimecode);
        
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

static string createMCClipFilename(string prefix, int64_t startTime, string suffix, int index)
{
    stringstream filename;
    
    filename << prefix << "_mc_" << startTime << "_"  << suffix << "_" << index << ".aaf";
        
    return filename.str();
}

static string createGroupSingleFilename(string prefix, string suffix)
{
    stringstream filename;
    
    filename << prefix << "_g_" << suffix << ".aaf";
        
    return filename.str();
}

static string createGroupMCFilename(string prefix, string suffix)
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
    fprintf(stderr, "  -p, --prefix <filepath>        Filename prefix used for AAF files (default '%s')\n", g_filenamePrefix);
    fprintf(stderr, "  -r --resolution <id>           Video resolution identifier as listed in the database\n"); 
    fprintf(stderr, "                                     (default is %d for DV-50)\n", DV50_MATERIAL_RESOLUTION);
    fprintf(stderr, "  -g, --group                    Create AAF file with all clips included\n");
    fprintf(stderr, "  -o, --grouponly                Only create AAF file with all clips included\n");
    fprintf(stderr, "      --no-ts-suffix             Don't use a timestamp for a group only file\n");
    fprintf(stderr, "  -m, --multicam                 Also create multi-camera clips\n");
    fprintf(stderr, "  -f, --from <date>S<timecode>   Includes clips created >= date and start timecode\n");
    fprintf(stderr, "  -t, --to <date>S<timecode>     Includes clips created < date and start timecode\n");
    fprintf(stderr, "  -c, --from-cd <date>T<time>    Includes clips created >= CreationDate of clip\n");
    fprintf(stderr, "  --tag <name>=<value>           Includes clips with user comment tag <name> == <value>\n");
    fprintf(stderr, "  --mc-cuts <db name>            Includes sequence of multi-camera cuts from database\n");
    fprintf(stderr, "  -d, --dns <string>             Database DNS (default '%s')\n", g_dns);
    fprintf(stderr, "  -u, --dbuser <string>          Database user name (default '%s')\n", g_databaseUserName);
    fprintf(stderr, "  --dbpassword <string>          Database user password (default ***)\n");
    fprintf(stderr, "  --xml			      Prints Apple XML for Final Cut Pro- disables AAF\n");
    fprintf(stderr, "  --fcp-path <string>	      Sets the path for FCP to see Ingex generated media\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "* --from and --to form is 'yyyy-mm-ddShh:mm:ss:ff'\n");
    fprintf(stderr, "* --from-cd form is 'yyyy-mm-ddThh:mm:ss'\n");
    fprintf(stderr, "* The default for --from is the start of today\n"); 
    fprintf(stderr, "* The default for --to is the start of tommorrow\n");
    fprintf(stderr, "* If --tag is used then --to and --from are ignored\n");
}


int main(int argc, const char* argv[])
{
    int cmdlnIndex = 1;
    string filenamePrefix = g_filenamePrefix;
    bool createGroup = false;
    bool createGroupOnly = false;
    bool createMultiCam = false;
    bool fcpxml = false;
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
    std::string mcCutsFilename;
    bool includeMCCutsSequence = false;
    CutsDatabase* mcCutsDatabase = 0;
    Timestamp fromCreationDate = g_nullTimestamp;
    string fcppath;
    
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
                createGroup = true;
                cmdlnIndex += 1;
            }
            else if (strcmp(argv[cmdlnIndex], "-o") == 0 ||
                strcmp(argv[cmdlnIndex], "--grouponly") == 0)
            {
                createGroupOnly = true;
                createGroup = true;
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
            else if (strcmp(argv[cmdlnIndex], "-f") == 0 ||
                strcmp(argv[cmdlnIndex], "--from") == 0)
            {
                if (cmdlnIndex + 1 >= argc)
                {
                    usage(argv[0]);
                    fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                    return 1;
                }
                parseDateAndTimecode(argv[cmdlnIndex + 1], &fromDate, &fromTimecode);
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
                parseDateAndTimecode(argv[cmdlnIndex + 1], &toDate, &toTimecode);
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
            else if (strcmp(argv[cmdlnIndex], "--xml") == 0)
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
                fcppath = argv[cmdlnIndex + 1];
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

    if (tagName.size() > 0)
    {
        suffix = createTagSuffix();
    }
    else
    {
        suffix = createDateAndTimecodeSuffix(fromDate, fromTimecode, toDate, toTimecode);
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
    auto_ptr<AAFFile> aafGroup;
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
            std::vector<prodauto::MaterialPackage *> packages_to_erase;
            Rational palEditRate = {25, 1};
            for (MaterialPackageSet::const_iterator
                it = material.topPackages.begin(); it != material.topPackages.end(); ++it)
            {
                MaterialPackage* topPackage = *it;
                
                if (topPackage->creationDate.year == fromDate.year && 
                    topPackage->creationDate.month == fromDate.month && 
                    topPackage->creationDate.day == fromDate.day &&
                    getStartTime(topPackage, material.packages, palEditRate) < fromTimecode)
                {
                    packages_to_erase.push_back(topPackage);
                }
                else if (topPackage->creationDate.year == toDate.year && 
                    topPackage->creationDate.month == toDate.month && 
                    topPackage->creationDate.day == toDate.day &&
                    getStartTime(topPackage, material.packages, palEditRate) >= toTimecode)
                {
                    packages_to_erase.push_back(topPackage);
                }
            }
            for (std::vector<prodauto::MaterialPackage *>::const_iterator
                it = packages_to_erase.begin(); it != packages_to_erase.end(); ++it)
            {
                material.topPackages.erase(*it);
            }
            unsigned int removedCount = packages_to_erase.size();

            if (verbose)
            {
                if (removedCount > 0)
                {
                    printf("Removed %d clips from those loaded that did not match the start timecode range\n", removedCount);
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
    
    // Additional debug code
    if (0) //(verbose)
    {
        printf("\nWe now have the following material packages...\n\n");
        for (MaterialPackageSet::iterator it = material.topPackages.begin(); it != material.topPackages.end(); ++it)
        {
            printf("%s\n", (*it)->toString().c_str());
        }
    }
    
    // go through the material package -> file package and remove any that reference
    // a file package with a non-zero videoResolutionID and !=  targetVideoResolutionID
    std::vector<prodauto::MaterialPackage *> packages_to_erase;
    for (MaterialPackageSet::const_iterator
        iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
    {
        MaterialPackage* topPackage = *iter1;
        
        bool erase = false;
        for (vector<Track*>::const_iterator
            iter2 = topPackage->tracks.begin(); iter2 != topPackage->tracks.end(); iter2++)
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
                    erase = true;
                    break; // break out of track loop
                }
            }

        }
        if (erase)
        {
            packages_to_erase.push_back(topPackage);
        }
    }
    for (std::vector<prodauto::MaterialPackage *>::const_iterator
        it = packages_to_erase.begin(); it != packages_to_erase.end(); ++it)
    {
        material.topPackages.erase(*it);
    }
    unsigned int removedCount = packages_to_erase.size();
    
    if (verbose)
    {
        if (removedCount > 0)
        {
            printf("Removed %d clips from those loaded that did not match the video resolution\n", removedCount);
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
    
    if (!fcpxml)
    {



    
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
            // create single source clips
            if (createGroup)
            {
                if (createGroupOnly && noTSSuffix)
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createUniqueFilename(filenamePrefix))));
                }
                else if (createMultiCam)
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createGroupMCFilename(filenamePrefix, suffix))));
                }
                else
                {
                    aafGroup = auto_ptr<AAFFile>(new AAFFile(
                        addFilename(filenames, createGroupSingleFilename(filenamePrefix, suffix))));
                }
            }
            MaterialPackageSet::iterator iter1;
            int index = 0;
            for (iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
            {
                MaterialPackage* topPackage = *iter1;
                
                if (!createGroupOnly)
                {
                    AAFFile aafFile(
                        addFilename(filenames, createSingleClipFilename(filenamePrefix, suffix, index++)));
                    aafFile.addClip(topPackage, material.packages);
                    aafFile.save();
                }
    
                if (createGroup)
                {
                    aafGroup->addClip(topPackage, material.packages);
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
                    Rational palEditRate = {25, 1}; // any rate will do
                    MaterialPackageSet::iterator iter2;
                    for (iter2 = iter1, iter2++; iter2 != material.topPackages.end(); iter2++)
                    {
                        MaterialPackage* topPackage2 = *iter2;
                        
                        if (topPackage2->creationDate.year == topPackage1->creationDate.year &&
                            topPackage2->creationDate.month == topPackage1->creationDate.month &&
                            topPackage2->creationDate.day == topPackage1->creationDate.day &&
                            getStartTime(topPackage1, material.packages, palEditRate) == 
                                getStartTime(topPackage2, material.packages, palEditRate))
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
                        if (mcCutsDatabase != 0)
                        {
                            sequence = getDirectorsCutSequence(mcCutsDatabase, mcClipDef, materialPackages, material.packages, 
                                topPackage1->creationDate, getStartTime(topPackage1, material.packages, palEditRate));
                        }
                        
                        if (!createGroupOnly)
                        {
                            AAFFile aafFile(
                                addFilename(filenames, createMCClipFilename(filenamePrefix, 
                                    getStartTime(topPackage1, material.packages, palEditRate), suffix, index++)));
                            aafFile.addMCClip(mcClipDef, materialPackages, material.packages, sequence);
                            aafFile.save();
                        }       
                        
                        if (createGroup)
                        {
                            aafGroup->addMCClip(mcClipDef, materialPackages, material.packages, sequence);
                        }

                        totalMulticamGroups++;
                        totalDirectorsCutsSequences = sequence.empty() ? 
                            totalDirectorsCutsSequences : totalDirectorsCutsSequences + 1;
                        printf("aaf directors cut sequence size = %d\n", sequence.size());
                    }
                } // iterate over material.topPackages
            }
            
            if (createGroup)
            {
                aafGroup->save();
                delete aafGroup.release();
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
    } //if not fcpxml

    else //if fcpxml
    {

    //printf("Making XML Doc...\n");

	if (material.topPackages.size() == 0)
	{
        printf("\n%s\n", g_resultsPrefix);
        printf("%d\n%d\n%d\n", totalClips, totalMulticamGroups, totalDirectorsCutsSequences);
	}
	else
	{
	    //printf("Packages available\n");
	    //Initialise XML DOM, declare Processing, Doctype, any comments etc..
	    static const char * newline = "\r\n";
	    static const char * newline_indent1 = "\r\n\t";
	    static const char * newline_indent2 = "\r\n\t\t";
	    static const char * newline_indent3 = "\r\n\t\t\t";
	    static const char * newline_indent4 = "\r\n\t\t\t\t";
	    static const char * newline_indent5 = "\r\n\t\t\t\t\t";
	    static const char * newline_indent6 = "\r\n\t\t\t\t\t\t";
	    static const char * newline_indent7 = "\r\n\t\t\t\t\t\t\t";
	    static const char * anamorph = "TRUE";
	    static const char * ntsc = "FALSE";
	    static const char * tb ="25";
	    static const char * df ="DF";
   	    static const char * src = "source";
	    int angle = 0;
	    std::string temptest = "Main";
	
	    // Initialize the XML4C2 system.
	    XmlTools::Initialise();
	    DOMDocument * doc;
	    DOMImplementation* impl =  DOMImplementationRegistry::getDOMImplementation(X("Core"));
	    doc = impl->createDocument(
                0,                  // root element namespace URI.
                X("xmeml"),         // root element name
                0);                 // document type object (DTD).

        DOMElement * rootElem = doc->getDocumentElement();
        rootElem->setAttribute(X("version"), X("2"));
        rootElem->appendChild(doc->createTextNode(X(newline)));

	    //Clip Definitions
        for (MaterialPackageSet::iterator iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
	    {

	        angle++;
            std::ostringstream angle_ss;
            angle_ss << angle; 

		//TAGS
		//Rate Tag
		DOMElement * rateElem = doc->createElement(X("rate"));
		//Timecode Tag
		DOMElement * timecodeElem = doc->createElement(X("timecode"));		
		//Duration
		DOMElement *durElem1 = doc->createElement(X("duration"));
		//Duration
		DOMElement *durElem = doc->createElement(X("duration"));
		//File Path
		DOMElement * purlElem = doc->createElement(X("pathurl"));
		//File Tag
		DOMElement * fileElem = doc->createElement(X("file"));
		//Media Tag
		DOMElement * mediaElem = doc->createElement(X("media"));
		//Video Tag
		DOMElement * videoElem = doc->createElement(X("video"));
		//Audio Tag
		DOMElement * audioElem = doc->createElement(X("audio"));
		
		//rate
		DOMElement * ntscElem = doc->createElement(X("ntsc"));
		ntscElem->appendChild(doc->createTextNode(X(ntsc)));
		
		//set rate properties
		DOMElement * tbElem = doc->createElement(X("timebase"));
		tbElem->appendChild(doc->createTextNode(X(tb)));
		rateElem->appendChild(doc->createTextNode(X(newline_indent2)));
		rateElem->appendChild(ntscElem);
		rateElem->appendChild(doc->createTextNode(X(newline_indent2)));
		rateElem->appendChild(tbElem);
		rateElem->appendChild(doc->createTextNode(X(newline_indent1)));

		//Open XML Clip
		DOMElement * clipElem = doc->createElement(X("clip"));
                rootElem->appendChild(doc->createTextNode(X(newline)));
                rootElem->appendChild(clipElem);
		clipElem->appendChild(doc->createTextNode(X(newline_indent1)));

		MaterialPackage * topPackage = *iter1;
		//printf("Package Name=%s\n",topPackage->name.c_str()); //package name: working
 
		DOMElement * nameElem = doc->createElement(X("name"));
		DOMElement * fnameElem = doc->createElement(X("name"));
		clipElem->setAttribute(X("id"), X(topPackage->name.c_str()));
                clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
                nameElem->appendChild(doc->createTextNode(X(topPackage->name.c_str())));
	

		DOMElement * anaElem = doc->createElement(X("anamorphic"));
		anaElem->appendChild(doc->createTextNode(X(anamorph)));

		//MEDIA INFO
		DOMElement * vsmplchElem = doc->createElement(X("samplecharacteristics"));
		DOMElement * asmplchElem = doc->createElement(X("samplecharacteristics"));	

		DOMElement * widthElem = doc->createElement(X("width"));
		widthElem->appendChild(doc->createTextNode(X("720")));

		DOMElement * heightElem = doc->createElement(X("height"));
		heightElem->appendChild(doc->createTextNode(X("576")));

		DOMElement * smplrteElem = doc->createElement(X("samplerate"));
		smplrteElem->appendChild(doc->createTextNode(X("48000")));

		DOMElement * dpthElem = doc->createElement(X("depth"));
		dpthElem->appendChild(doc->createTextNode(X("16")));

		DOMElement * layoutElem = doc->createElement(X("layout"));
		layoutElem->appendChild(doc->createTextNode(X("stereo")));
	
		DOMElement * inElem = doc->createElement(X("in"));
		inElem->appendChild(doc->createTextNode(X("-1")));

		DOMElement * outElem = doc->createElement(X("out"));
		outElem->appendChild(doc->createTextNode(X("-1")));

		DOMElement * ismasterElem = doc->createElement(X("ismasterclip"));
		ismasterElem->appendChild(doc->createTextNode(X("TRUE")));

		//Audio Channel Tag- right

		DOMElement * chnlElem = doc->createElement(X("channelcount"));

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
                
                st_time = getStartTime(topPackage, material.packages, palEditRate); //start frames from midnite
		//printf("Package Start Time=%lld\n",st_time);
		Timecode tc(st_time);  //returns true time code
                //printf("Time code=%s\n",tc.Text());

		//Rate details: Timecode, Display Format, Source
		DOMElement * tcElem = doc->createElement(X("string"));
		tcElem->appendChild(doc->createTextNode(X(tc.Text())));

		DOMElement * dfElem = doc->createElement(X("displayformat"));
		dfElem->appendChild(doc->createTextNode(X(df)));
		
		DOMElement * scElem = doc->createElement(X("source"));
		scElem->appendChild(doc->createTextNode(X(src)));

		//Start of DOM
			//name
			clipElem->appendChild(nameElem);
			//duration
        	        clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
			clipElem->appendChild(durElem1);
			//anamorphic
        	        clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
			clipElem->appendChild(anaElem);
			//rate
			clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
			clipElem->appendChild(rateElem);

			//file name, tc, path into file
			fileElem->appendChild(doc->createTextNode(X(newline_indent2)));
			fileElem->appendChild(fnameElem);
                	fileElem->appendChild(doc->createTextNode(X(newline_indent2)));
			fileElem->appendChild(purlElem);
                	fileElem->appendChild(doc->createTextNode(X(newline_indent2)));
			fileElem->appendChild(tcElem);
                	//file into clip
			fileElem->setAttribute(X("id"), X(topPackage->name.c_str()));
			clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
			clipElem->appendChild(fileElem);

			//timecode inside file
			timecodeElem->appendChild(doc->createTextNode(X(newline_indent3)));
			timecodeElem->appendChild(tcElem);
			timecodeElem->appendChild(doc->createTextNode(X(newline_indent3)));
			timecodeElem->appendChild(dfElem);
			timecodeElem->appendChild(doc->createTextNode(X(newline_indent3)));
			timecodeElem->appendChild(scElem);
			timecodeElem->appendChild(doc->createTextNode(X(newline_indent2)));
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
		            durElem->appendChild(doc->createTextNode(X(dur_ss.str().c_str())));
		            durElem1->appendChild(doc->createTextNode(X(dur_ss.str().c_str())));
		            purlElem->appendChild(doc->createTextNode(X(fcppath.c_str())));
		            purlElem->appendChild(doc->createTextNode(X(filelocation.c_str())));
			    fnameElem->appendChild(doc->createTextNode(X(trackname.c_str())));
		            //duration into file->media->video
		            videoElem->appendChild(doc->createTextNode(X(newline_indent4)));		
		            videoElem->appendChild(durElem);
		            videoElem->appendChild(doc->createTextNode(X(newline_indent4)));
		            //width and height into video sample characterstics
		            vsmplchElem->appendChild(doc->createTextNode(X(newline_indent5)));
		            vsmplchElem->appendChild(widthElem);
		            vsmplchElem->appendChild(doc->createTextNode(X(newline_indent5)));
		            vsmplchElem->appendChild(heightElem);				
		            vsmplchElem->appendChild(doc->createTextNode(X(newline_indent4)));
		            //video sample characteristics into file->media->video
		            videoElem->appendChild(vsmplchElem);
		            videoElem->appendChild(doc->createTextNode(X(newline_indent3)));
	  	            //video into media
		            mediaElem->appendChild(doc->createTextNode(X(newline_indent3)));
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
			             asmplchElem->appendChild(doc->createTextNode(X(newline_indent5)));
			             asmplchElem->appendChild(smplrteElem);
			             asmplchElem->appendChild(doc->createTextNode(X(newline_indent5)));
			             asmplchElem->appendChild(dpthElem);				
			             asmplchElem->appendChild(doc->createTextNode(X(newline_indent4)));
			             //audio sample characteristics into file->media->audio
			             audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			             audioElem->appendChild(asmplchElem);
			             //layout into audio
			             audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			             audioElem->appendChild(layoutElem);
			         }
			         //int t=taudio;
			         //if(t == 1)
			         //{			
			             std::ostringstream t1_ss;
			             t1_ss << taudio;			
			             //audio channel label into audio channel
			             {
			               DOMElement * chnllbrElem = doc->createElement(X("channellabel"));
			               DOMElement * audiochrElem1 = doc->createElement(X("audiochannel"));
			               audiochrElem1->appendChild(doc->createTextNode(X(newline_indent5)));
			               chnllbrElem->appendChild(doc->createTextNode(X("ch")));
			               chnllbrElem->appendChild(doc->createTextNode(X(t1_ss.str().c_str())));
			               audiochrElem1->appendChild(chnllbrElem);
			               audiochrElem1->appendChild(doc->createTextNode(X(newline_indent4)));
			               //audio channel into file->media->audio
			               audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			               audioElem->appendChild(audiochrElem1);
			               audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			             }
			             mediaElem->appendChild(doc->createTextNode(X(newline_indent3)));
			             mediaElem->appendChild(audioElem) ;
			             //t--;
			         //}
			         /*if(t == 2)
			         {			
			             std::ostringstream t1_ss;
			             t1_ss << t;			
			             //audio channel label into audio channel
			             {
			               DOMElement * chnllbrElem = doc->createElement(X("channellabel"));
			               DOMElement * audiochrElem1 = doc->createElement(X("audiochannel"));
			               audiochrElem1->appendChild(doc->createTextNode(X(newline_indent5)));
			               chnllbrElem->appendChild(doc->createTextNode(X("ch")));
			               chnllbrElem->appendChild(doc->createTextNode(X(t1_ss.str().c_str())));
			               audiochrElem1->appendChild(chnllbrElem);
			               audiochrElem1->appendChild(doc->createTextNode(X(newline_indent4)));
			               //audio channel into file->media->audio
			               audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			               audioElem->appendChild(audiochrElem1);
			               audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
			             }
			             mediaElem->appendChild(doc->createTextNode(X(newline_indent3)));
			             mediaElem->appendChild(audioElem) ;
			             t--;
			         }*/
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
		 chnlElem->appendChild(doc->createTextNode(X("2")));
		 audioElem->appendChild(doc->createTextNode(X(newline_indent4)));
		 audioElem->appendChild(chnlElem);	
		 //media into file
		 mediaElem->appendChild(doc->createTextNode(X(newline_indent2)));
		 fileElem->appendChild(doc->createTextNode(X(newline_indent2)));
		 fileElem->appendChild(mediaElem);
		 fileElem->appendChild(doc->createTextNode(X(newline_indent1)));	
		 //clip details cntd.
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
		 clipElem->appendChild(inElem);
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
		 clipElem->appendChild(outElem);
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
		 //master clip id, name plus M
		 DOMElement * masteridElem = doc->createElement(X("masterclipid"));
		 masteridElem->appendChild(doc->createTextNode(X("M")));
		 masteridElem->appendChild(doc->createTextNode(X(topPackage->name.c_str())));
		 clipElem->appendChild(masteridElem);
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
		 clipElem->appendChild(ismasterElem);
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));
		
		 //default angle, must increase by 1 for each clip
		 DOMElement * defangleElem = doc->createElement(X("defaultangle"));
		 defangleElem->appendChild(doc->createTextNode(X(angle_ss.str().c_str())));
		 clipElem->appendChild(defangleElem);			
		 clipElem->appendChild(doc->createTextNode(X(newline_indent1)));

		 clipElem->appendChild(doc->createTextNode(X(newline)));
		 clipElem->appendChild(doc->createTextNode(X(newline)));	
	         ++ totalClips;	
	    }// End of Clip definitions
                
	if (createMultiCam)
        {	

	    int n=0;
	    // load multi-camera clip definitions- need MCClip defs(?)
            vector<MCClipDef*>::iterator itmc;
	    MaterialPackageSet materialPackages;
            for (itmc = mcClipDefs.get().begin(); itmc != mcClipDefs.get().end(); itmc++)
	    {
	        MaterialPackageSet donePackages;
	        for (MaterialPackageSet::const_iterator iter1 = material.topPackages.begin(); iter1 != material.topPackages.end(); iter1++)
                {
	            //De-bugging info
	            //printf("\n");
	            //printf("--------- START OF MULTICLIP ---------\n");
	            ++n;
	            std::ostringstream n_ss;
                    n_ss << n;
	            //printf("Multiclip \n");       
  	            int c1 = 0;
	            //DOM Details for Multiclip
	            //TAGS
	            //Rate Tag
	            DOMElement * rateElem = doc->createElement(X("rate"));
	            DOMElement * rate2Elem = doc->createElement(X("rate"));
	            //Media Tag
	            DOMElement * mediaElem = doc->createElement(X("media"));
	            //Video Tag
	            DOMElement * videoElem = doc->createElement(X("video"));
 	            //rate
	            DOMElement * ntscElem = doc->createElement(X("ntsc"));
	            ntscElem->appendChild(doc->createTextNode(X(ntsc)));
	            DOMElement * tbElem = doc->createElement(X("timebase"));
	            tbElem->appendChild(doc->createTextNode(X(tb)));
	            rateElem->appendChild(doc->createTextNode(X(newline_indent2)));
	            rateElem->appendChild(ntscElem);
	            rateElem->appendChild(doc->createTextNode(X(newline_indent2)));
	            rateElem->appendChild(tbElem);
	            rateElem->appendChild(doc->createTextNode(X(newline_indent1)));

	            DOMElement * ntscElem2 = doc->createElement(X("ntsc"));
	            ntscElem2->appendChild(doc->createTextNode(X(ntsc)));
	            DOMElement * tbElem2 = doc->createElement(X("timebase"));
	            tbElem2->appendChild(doc->createTextNode(X(tb)));
	            rate2Elem->appendChild(doc->createTextNode(X(newline_indent5)));
	            rate2Elem->appendChild(ntscElem2);
	            rate2Elem->appendChild(doc->createTextNode(X(newline_indent5)));
	            rate2Elem->appendChild(tbElem2);
	            rate2Elem->appendChild(doc->createTextNode(X(newline_indent4)));
	    
                    //Mduration
	            DOMElement * MdurElem = doc->createElement(X("duration"));
	            DOMElement * Mdur2Elem = doc->createElement(X("duration"));
	            //Mclipitem1
	            DOMElement * Mclipitem1Elem = doc->createElement(X("clipitem"));
	            //Multiclip id
	            DOMElement * MclipidElem = doc->createElement(X("multiclip"));
	            //Mcollapsed
	            DOMElement * McollapsedElem = doc->createElement(X("collapsed"));
	            //Msynctype
	            DOMElement * MsyncElem = doc->createElement(X("synctype"));
	            //Mclip
	            DOMElement * MclipElem = doc->createElement(X("clip"));
	            //Mname
	            DOMElement * MnameElem = doc->createElement(X("name"));
	            DOMElement * Mname1Elem = doc->createElement(X("name"));
	            DOMElement * Mname2Elem = doc->createElement(X("name"));
	            //Track Tag
	            DOMElement * trackElem = doc->createElement(X("track"));

	            DOMElement * inElem = doc->createElement(X("in"));
	            inElem->appendChild(doc->createTextNode(X("-1")));

	            DOMElement * outElem = doc->createElement(X("out"));
	            outElem->appendChild(doc->createTextNode(X("-1")));

	            DOMElement * ismasterElem = doc->createElement(X("ismasterclip"));
	            ismasterElem->appendChild(doc->createTextNode(X("FALSE")));

	            //Name
                    MaterialPackage* topPackage1 = *iter1;
                    if (!donePackages.insert(topPackage1).second)
                    {
                    // already done this package
                       continue;
                    }
                   
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
			  
		           materialPackages.insert(topPackage2);
			   donePackages.insert(topPackage2);
			   //looks in at Track Detail
			   for (std::vector<prodauto::Track*>::const_iterator iter2 = topPackage2->tracks.begin(); iter2 != topPackage2->tracks.end(); iter2++) 
			   {
			       c1++;
			       prodauto::Track * track = *iter2;
			       if (c1 == 1)
			       {
			           //MClip DOM Start
				   rootElem->appendChild(doc->createTextNode(X(newline)));
				   rootElem->appendChild(MclipElem);
				   MclipElem->setAttribute(X("id"), X(n_ss.str().c_str()));
				   //Name
				   Mname2Elem->appendChild(doc->createTextNode(X("Multicam")));
				   Mname2Elem->appendChild(doc->createTextNode(X(n_ss.str().c_str())));
				   Mname1Elem->appendChild(doc->createTextNode(X("Multicam")));
				   Mname1Elem->appendChild(doc->createTextNode(X(n_ss.str().c_str())));
				   MnameElem->appendChild(doc->createTextNode(X("Multicam")));
				   MnameElem->appendChild(doc->createTextNode(X(n_ss.str().c_str())));
				   //Duration
				   std::ostringstream tMdurss;
				   tMdurss << track->sourceClip->length;
				   MdurElem->appendChild(doc->createTextNode(X(tMdurss.str().c_str())));
				   Mdur2Elem->appendChild(doc->createTextNode(X(tMdurss.str().c_str())));
				   //Clip Items: name
				   rootElem->appendChild(doc->createTextNode(X(newline)));
				   Mclipitem1Elem->setAttribute(X("id"), X(n_ss.str().c_str()));
				   //Collapsed
				   McollapsedElem->appendChild(doc->createTextNode(X("FALSE")));
				   //Sync Type
				   MsyncElem->appendChild(doc->createTextNode(X("3")));
				   //Append above to Tracks
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   MclipElem->appendChild(Mname1Elem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   MclipElem->appendChild(MdurElem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   //Rate
				   MclipElem->appendChild(rateElem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   MclipElem->appendChild(inElem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   MclipElem->appendChild(outElem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   MclipElem->appendChild(ismasterElem);
				   MclipElem->appendChild(doc->createTextNode(X(newline_indent1)));
				   //Multiclip
				   Mclipitem1Elem->appendChild(doc->createTextNode(X(newline_indent4)));
				   Mclipitem1Elem->appendChild(Mname2Elem);
				   Mclipitem1Elem->appendChild(doc->createTextNode(X(newline_indent4)));
				   Mclipitem1Elem->appendChild(Mdur2Elem);
				   Mclipitem1Elem->appendChild(doc->createTextNode(X(newline_indent4)));
				   Mclipitem1Elem->appendChild(rate2Elem);
				   Mclipitem1Elem->appendChild(doc->createTextNode(X(newline_indent4)));
				   Mclipitem1Elem->setAttribute(X("id"), X(n_ss.str().c_str()));
				   MclipidElem->appendChild(doc->createTextNode(X(newline_indent4)));
				   MclipidElem->appendChild(MnameElem);
				   MclipidElem->appendChild(doc->createTextNode(X(newline_indent4)));
				   MclipidElem->appendChild(McollapsedElem);
				   MclipidElem->appendChild(doc->createTextNode(X(newline_indent4)));
				   MclipidElem->appendChild(MsyncElem);
				   MclipidElem->appendChild(doc->createTextNode(X(newline_indent4)));
				   MclipidElem->setAttribute(X("id"), X(n_ss.str().c_str()));
			        }
			        //Material Package Name
			        SourcePackage dummy;
			        dummy.uid = track->sourceClip->sourcePackageUID;
			        PackageSet::iterator result = material.packages.find(&dummy);
			        vector<MCClipDef*>::iterator itmc1;
			        MaterialPackageSet materialPackages;
			        for (itmc1 = mcClipDefs.get().begin(); itmc1 != mcClipDefs.get().end(); itmc1++)
			        {
			            MCClipDef * mcClipDef = *itmc1;				
				    if (itmc1 == itmc)
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
					            DOMElement * Mclip2Elem = doc->createElement(X("clip"));
					            //Mclip
					            DOMElement * MfileElem = doc->createElement(X("file"));
					            //Mangle1
					            DOMElement * Mangle1Elem = doc->createElement(X("angle"));
					            //printf("Video Track Name=%s\n",track->name.c_str()); //track name
					            //printf("Video Source Clip Length=%lld\n",track->sourceClip->length);
						    Mclip2Elem->setAttribute(X("id"), X(topPackage2->name.c_str()));	
						    MfileElem->setAttribute(X("id"), X(topPackage2->name.c_str()));
						    Mangle1Elem->appendChild(doc->createTextNode(X(newline_indent6)));
						    Mclip2Elem->appendChild(doc->createTextNode(X(newline_indent7)));
						    Mclip2Elem->appendChild(MfileElem);
						    Mclip2Elem->appendChild(doc->createTextNode(X(newline_indent7)));
						    Mangle1Elem->appendChild(Mclip2Elem);
						    Mangle1Elem->appendChild(doc->createTextNode(X(newline_indent6)));
						    MclipidElem->appendChild(doc->createTextNode(X(newline_indent5)));
						    MclipidElem->appendChild(Mangle1Elem);
						    Mclipitem1Elem->appendChild(MclipidElem);
						    Mclipitem1Elem->appendChild(doc->createTextNode(X(newline_indent4)));
						    trackElem->appendChild(Mclipitem1Elem);
						    trackElem->appendChild(doc->createTextNode(X(newline_indent3)));
						    videoElem->appendChild(trackElem);
						    videoElem->appendChild(doc->createTextNode(X(newline_indent2)));
						    mediaElem->appendChild(videoElem);
						    mediaElem->appendChild(doc->createTextNode(X(newline_indent2)));
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

	    ++ totalMulticamGroups;
		}

	    }	
	    //printf("--------- END OF MULTICLIP ---------\n");

	}// end of multicam  



		    //SEQUENCES

		    //use material packges set
/*		    printf("here?!\n");

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

            vector<Track*>::const_iterator iter2;
            for (iter2 = materialPackage->tracks.begin(); iter2 != materialPackage->tracks.end(); iter2++)
            {
                Track* track = *iter2;
                
                // get the source package and track for this material package track
                if (getSource(track, packages, &fileSourcePackage, &sourcePackage, &sourceTrack))
                {
                    bool donePackage = false;
                    map<uint32_t, MCTrackDef*>::const_iterator iter3;
                    for (iter3 = mcClipDef->trackDefs.begin(); !donePackage && iter3 != mcClipDef->trackDefs.end(); iter3++)
                    {
                        MCTrackDef* trackDef = (*iter3).second;
                        
                        map<uint32_t, MCSelectorDef*>::const_iterator iter4;
                        for (iter4 = trackDef->selectorDefs.begin(); !donePackage && iter4 != trackDef->selectorDefs.end(); iter4++)
                        {
                            MCSelectorDef* selectorDef = (*iter4).second;
                        
                            if (!selectorDef->isConnectedToSource())
                            {
                                continue;
                            }
                            
                            // if the source package and track match the multi-camera selection
                            if (selectorDef->sourceConfig->name.compare(fileSourcePackage->sourceConfigName) == 0 && 
                                sourceTrack->id == selectorDef->sourceTrackID)
                            {
                                if (activeMaterialPackages.insert(materialPackage).second)
                                {
                                    printf("sourceConfig name %s\n", selectorDef->sourceConfig->name.c_str());
				    printf("fileConfig name %s\n", fileSourcePackage->sourceConfigName.c_str());
                                }
                                donePackage = true;
                            }
                        }
                    }
                }
	    
*/
/*		   //load sequence 
	vector<MCClipDef*>::iterator iter6;
	printf("im here1\n");
        MaterialPackageSet materialPackages;
	for (iter6 = mcClipDefs.get().begin(); iter6 != mcClipDefs.get().end(); iter6++)
	{
	    MCClipDef* mcClipDef = *iter6;
	    vector<CutInfo> sequence;
	    sequence = getDirectorsCutSequence(mcCutsDatabase, mcClipDef, materialPackages, material.packages, topPackage1->creationDate, getStartTime(topPackage1, material.packages, palEditRate));
	    printf("xml directors cut sequence size = %d\n", sequence.size());
	    printf("im here2\n");
	    //loads packages to comare in topPackages10

	    for (MaterialPackageSet::const_iterator iter10 = material.topPackages.begin(); iter10 != material.topPackages.end(); iter10++)
            {
		//vector<MaterialPackage*>::iterator iter10;
                MaterialPackage* topPackage10 = *iter10;
                materialPackages.insert(topPackage10);
		    
                Rational palEditRate = {25, 1}; 
	printf("im here 3\n");
		//loads packages to comare in topPackages11
                for (MaterialPackageSet::const_iterator iter11 = material.topPackages.begin(); iter11 != material.topPackages.end(); iter11++)
                {	
		    MaterialPackage* topPackage11 = *iter11;
		    materialPackages.insert(topPackage11);

	printf("im here 4\n");
                    if (mcCutsDatabase != 0)
                    {
	   		//compares multiclip times
			if (topPackage11->creationDate.year == topPackage10->creationDate.year && topPackage11->creationDate.month == topPackage10->creationDate.month && topPackage11->creationDate.day == topPackage10->creationDate.day && getStartTime(topPackage10, material.packages, palEditRate) ==  getStartTime(topPackage11, material.packages, palEditRate))
			{		
			    //goes through SourceConfig-(Clips)
			    for (std::vector<SourceConfig*>::const_iterator iter13 = mcClipDef->sourceConfigs.begin(); iter13 != mcClipDef->sourceConfigs.end(); iter13++)
			    {
	printf("im here 5\n");
			        //goes through CutInfos.
			        for (vector<CutInfo>::const_iterator it = sequence.begin(); it != sequence.end(); ++it)
			        {
	printf("im here 6\n");					
				    if(it->source == (*iter13)->name)
				    {
				        int db_id = (*iter13)->getID();
					printf("Source Config name %s\n",(*iter13)->name.c_str());
				    	printf("position %lld\n",it->position);
				    	printf("video source %s\n",it->source.c_str());
					printf("databaseid (which is useless!) %d\n",db_id);
					printf("type %d\n", (*iter13)->type);
				   	printf("track configs %s\n" , (*iter13)->trackConfigs->name.c_str());
					printf("____________\n");
				    }
				//printf("Seq details %s\n", sequence.source());
				}		
			    }
			}
			else
	 	        {
			    printf("now your confused\n");
			}
                    }
		}
            }
        }            
*/		/*
		    for (std::vector<SourceConfig*>::const_iterator it6 = MCClipDef.sourceConfigs.begin(); it6 != MCClipDef.sourceConfigs.end(); it6++)
		    {
			printf("sourceConfigs name%s\n", it6->name.c_str());
		    }

			for (iter6 = mcClipDefs.get().begin(); iter6 != mcClipDefs.get().end(); iter6++)
			{
			    MCClipDef* mcClipDef = *iter6;
			    vector<CutsDatabaseEntry> seq;
                            vector<CutInfo> sequence;
			    if (mcCutsDatabase == 0)
			    {
			        printf("No Details in mcCuts Database\n");
			    }
			    else //(mcCutsDatabase !=0)
			    {
				//seq= getDirectorsCutSequence(mcCutsDatabase, mcClipDef, materialPackages, material.packages, topPackage1->creationDate, getStartTime(topPackage1, material.packages, palEditRate));
				printf("We're in action- bring it on!\n");
				sequence = getDirectorsCutSequence(mcCutsDatabase, mcClipDef, materialPackages, material.packages, topPackage1->creationDate, getStartTime(topPackage1, material.packages, palEditRate));
				printf("xml directors cut sequence size = %d\n", sequence.size());

				//DOM SEQ START DETAILS
				//MClip DOM Start

			for (std::vector<SourceConfig*>::const_iterator it6 = MCClipDef.sourceConfigs.begin(); it6 != MCClipDef.sourceConfigs.end(); it6++)
		  	   {
			        printf("sourceConfigs name%s\n", it6->name.c_str());
		    	   }

				
				rootElem->appendChild(doc->createTextNode(X(newline)));
				rootElem->appendChild(seqElem);
				seqElem->setAttribute(X("id"), X("Multicam Sequence"));
				seqElem->appendChild(doc->createTextNode(X(newline_indent1)));
				seqNElem->appendChild(doc->createTextNode(X("Directors Cut")));
				seqElem->appendChild(seqNElem);
				seqElem->appendChild(doc->createTextNode(X(newline_indent1)));
				seqRElem->appendChild(doc->createTextNode(X(newline_indent2)));
				seqRElem->appendChild(seqTElem);
				seqElem->appendChild(seqRElem);
				//extra stuff goes here			


	
				for (vector<CutInfo>::const_iterator it = sequence.begin(); it != sequence.end(); ++it)
				{
				    printf("position %lld\n",it->position);
				    printf("video source %s\n",it->source.c_str());
				

				for (std::vector<CutsDatabaseEntry>::const_iterator it2 = sequence.begin(); it2 != sequence.end(); ++it2)
				{
				    printf("source id %s\n",it2->sourceId.c_str());
				} 
				    
                		    for (std::vector<prodauto::Track*>::const_iterator iter7 = topPackage1->tracks.begin(); iter7 != topPackage1->tracks.end(); iter7++) //looks in at Track Detail
				    {
					
		                        prodauto::Track * track2 = *iter7;
					if (track2->dataDef == PICTURE_DATA_DEFINITION) //video only
					{


		    			SourcePackage dummy;
                   			dummy.uid = track2->sourceClip->sourcePackageUID;
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

		    			    printf("Track Name=%s\n",track2->name.c_str());
                    			    printf("Source Clip Length=%lld\n",track2->sourceClip->length); 
		    		   	    printf("Picture Def=%d\n",track2->dataDef);

					    FileEssenceDescriptor* fileDescriptor2 = dynamic_cast<FileEssenceDescriptor*>(sourcePackage->descriptor);
					    printf("Location=%s\n",fileDescriptor2->fileLocation.c_str()); //file Location
					    printf("File Format=%d\n",fileDescriptor2->fileFormat); // file Format
					    printf("==================\n");
					   //DOM- Clip details are here!
					   //
					}
					}
				    }
				}
			    }
			    
			if (!createGroupOnly)
			{
			//Make Seq Here
			AAFFile aafFile(addFilename(filenames, createMCClipFilename(filenamePrefix, getStartTime(topPackage1, material.packages, palEditRate), suffix, index++)));
			aafFile.addMCClip(mcClipDef, materialPackages, material.packages, sequence);
			aafFile.save();
			}       
			
			if (createGroup)
			{
			//Make -- Don't know what this does!
			aafGroup->addMCClip(mcClipDef, materialPackages, material.packages, sequence);
			}

			totalMulticamGroups++;
			totalDirectorsCutsSequences = sequence.empty() ? 
			totalDirectorsCutsSequences : totalDirectorsCutsSequences + 1; 
			}                         
		*/	
		
//DOM Print Details
   
        XmlTools::DomToFile(doc, filenamePrefix.c_str());
        doc->release();
        XmlTools::Terminate();
        //printf("its out\n");
        printf("\n%s\n", g_resultsPrefix);
        printf("%d\n%d\n%d\n", totalClips, totalMulticamGroups, totalDirectorsCutsSequences);
        vector<string>::const_iterator filenamesIter;
        printf("%s\n",filenamePrefix.c_str());
    }//CLOSES XML DOM

    }//CLOSES IF FCPXML

    Database::close();
    return 0;
}




