/*
 * $Id: import_cuts.cpp,v 1.1 2009/10/12 18:47:10 john_f Exp $
 *
 * Import Mult-Camera Cuts and Clip Definitions from XML file
 * to database.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <vector>
#include <map>

#include <Database.h>
#include <ProdAutoException.h>

#include "XMCFile.h"
#include "ICException.h"

using namespace std;
using namespace prodauto;


const char * const DEFAULT_DBHOST = "localhost";
const char * const DEFAULT_DBNAME = "prodautodb";
const char * const DEFAULT_DBUSER = "bamzooki";
const char * const DEFAULT_DBPW = "bamzooki";



static void create_and_save_db_location(Database *db, map<long, string> &db_locations, string name)
{
    long db_id = db->saveLiveRecordingLocation(name);
    printf("Imported live recording location '%s'\n", name.c_str());
    
    db_locations.insert(make_pair(db_id, name));
}

static long get_location_id(map<long, string> &locations, string name, bool assert_check = true)
{
    map<long, string>::iterator iter;
    for (iter = locations.begin(); iter != locations.end(); iter++) {
        if (iter->second == name)
            return iter->first;
    }
    
    if (assert_check)
        throw ICException("Unknown recording location '%s'", name.c_str());
    
    return -1;
}

static map<long, string> import_or_load_locations(Database *db, XMCFile *x_file)
{
    map<long, string> db_locations;
    
    db_locations = db->loadLiveRecordingLocations();
    
    set<XLocation*, XLocationComparator>::iterator iter;
    for (iter = x_file->Locations().begin(); iter != x_file->Locations().end(); iter++) {

        if (get_location_id(db_locations, (*iter)->Name(), false) >= 0)
            continue; // is persistent

        create_and_save_db_location(db, db_locations, (*iter)->Name());
    }
    
    return db_locations;
}

static SourceConfig* create_and_save_db_source_config(Database *db, map<long, std::string> &locations,
                                                      XSourceConfig *x_src_config)
{
    auto_ptr<SourceConfig> db_src_config(new SourceConfig());
    db_src_config->name = x_src_config->Name();
    db_src_config->type = LIVE_SOURCE_CONFIG_TYPE;
    db_src_config->recordingLocation = get_location_id(locations, x_src_config->LocationName());
    
    map<int, XSourceTrackConfig*>& x_tracks = x_src_config->Tracks();
    map<int, XSourceTrackConfig*>::iterator x_track_iter;
    for (x_track_iter = x_tracks.begin(); x_track_iter != x_tracks.end(); x_track_iter++) {
        auto_ptr<SourceTrackConfig> db_track(new SourceTrackConfig());
        
        db_track->id = x_track_iter->second->Id();
        db_track->name = x_track_iter->second->Name();
        db_track->number = x_track_iter->second->Number();
        db_track->dataDef = x_track_iter->second->DataDef();
        db_track->editRate = g_palEditRate;
        db_track->length = 10800000;

        db_src_config->trackConfigs.push_back(db_track.release());
    }
    
    db->saveSourceConfig(db_src_config.get());
    printf("Imported source config '%s'\n", db_src_config->name.c_str());
    
    return db_src_config.release();
}

static map<string, SourceConfig*> import_or_load_source_configs(Database *db, map<long, std::string> &locations,
                                                                XMCFile *x_file)
{
    map<string, SourceConfig*> db_src_configs;
    SourceConfig *db_src_config;
    
    map<std::string, XSourceConfig*>::iterator iter;
    for (iter = x_file->SourceConfigs().begin(); iter != x_file->SourceConfigs().end(); iter++) {
        
        db_src_config = db->loadSourceConfig(iter->second->Name(), false);
        if (!db_src_config)
            db_src_config = create_and_save_db_source_config(db, locations, iter->second);
        
        db_src_configs.insert(make_pair(db_src_config->name, db_src_config));
        // no need to check whether it was inserted because the name was checked for uniqueness when reading
        // the XML clip
    }
    
    return db_src_configs;
}

static SourceConfig* get_source_config(Database *db, map<string, SourceConfig*> &src_configs, MCClipDef *db_clip,
                                       string source_config_name)
{
    // try the source configs in the clip def
    vector<SourceConfig*>::iterator config_iter;
    for (config_iter = db_clip->sourceConfigs.begin(); config_iter != db_clip->sourceConfigs.end(); config_iter++) {
        if ((*config_iter)->name == source_config_name) {
            return *config_iter;
        }
    }
    
    // try the source configs defined in the xml doc
    map<string, SourceConfig*>::iterator src_config_iter = src_configs.find(source_config_name);
    if (src_config_iter != src_configs.end()) {
        db_clip->sourceConfigs.push_back(src_config_iter->second);
        // db_clip has taken ownership of the source config
        src_configs.erase(src_config_iter);
        return db_clip->sourceConfigs.back();
    }
    
    // load the source config from the database
    db_clip->sourceConfigs.push_back(db->loadSourceConfig(source_config_name));
    return db_clip->sourceConfigs.back();
}

static MCClipDef* create_and_save_db_clip(Database *db, map<string, SourceConfig*> &src_configs, XMCClip *x_clip)
{
    auto_ptr<MCClipDef> db_clip(new MCClipDef());
    db_clip->name = x_clip->Name();
    
    map<int, XMCTrack*>& x_tracks = x_clip->Tracks();
    map<int, XMCTrack*>::iterator x_track_iter;
    for (x_track_iter = x_tracks.begin(); x_track_iter != x_tracks.end(); x_track_iter++) {
        auto_ptr<MCTrackDef> db_track(new MCTrackDef());
        
        db_track->index = x_track_iter->second->Index();
        db_track->number = x_track_iter->second->Number();

        map<int, XMCSelector*>& x_selectors = x_track_iter->second->Selectors();
        map<int, XMCSelector*>::iterator x_selector_iter;
        for (x_selector_iter = x_selectors.begin(); x_selector_iter != x_selectors.end(); x_selector_iter++) {
            auto_ptr<MCSelectorDef> db_selector(new MCSelectorDef());
            
            db_selector->index = x_selector_iter->second->Index();
            
            db_selector->sourceConfig = get_source_config(db, src_configs, db_clip.get(),
                                                          x_selector_iter->second->SourceConfigName());
            db_selector->sourceTrackID = x_selector_iter->second->SourceTrackConfigId();
            
            db_track->selectorDefs.insert(make_pair(db_selector->index, db_selector.get()));
            // no need to check whether it was inserted because the index was checked for uniqueness when reading
            // the XML selector
            db_selector.release();
        }
        
        db_clip->trackDefs.insert(make_pair(db_track->index, db_track.get()));
        // no need to check whether it was inserted because the index was checked for uniqueness when reading
        // the XML track
        db_track.release();
    }
    
    db->saveMultiCameraClipDef(db_clip.get());
    printf("Imported multi-camera clip definition '%s'\n", db_clip->name.c_str());
    
    return db_clip.release();
}

static map<string, MCClipDef*> import_or_load_clip_defs(Database *db, map<string, SourceConfig*> &src_configs,
                                                        XMCFile *x_file)
{
    map<string, MCClipDef*> db_clips;
    MCClipDef *db_clip;
    
    map<std::string, XMCClip*>::iterator iter;
    for (iter = x_file->Clips().begin(); iter != x_file->Clips().end(); iter++) {
        
        db_clip = db->loadMultiCameraClipDef(iter->second->Name(), false);
        if (!db_clip)
            db_clip = create_and_save_db_clip(db, src_configs, iter->second);
        
        db_clips.insert(make_pair(db_clip->name, db_clip));
        // no need to check whether it was inserted because the name was checked for uniqueness when reading
        // the XML clip
    }
    
    return db_clips;
}

static MCClipDef* get_db_clip(map<string, MCClipDef*> &db_clips, string name)
{
    map<string, MCClipDef*>::iterator db_clip_iter = db_clips.find(name);
    if (db_clip_iter == db_clips.end())
        throw ICException("Unknown multi-camera clip def '%s'", name.c_str());
    
    return db_clip_iter->second;
}

static MCTrackDef* get_assumed_db_track(MCClipDef *db_clip)
{
    MCTrackDef* db_track = 0;
    bool done = false;
    SourceTrackConfig *track_config;

    // return the video track, otherwise return the first audio track
    
    map<uint32_t, MCTrackDef*>::iterator track_iter;
    for (track_iter = db_clip->trackDefs.begin(); !done && track_iter != db_clip->trackDefs.end(); track_iter++) {
        map<uint32_t, MCSelectorDef*>::iterator selector_iter;
        for (selector_iter = track_iter->second->selectorDefs.begin();
             !done && selector_iter != track_iter->second->selectorDefs.end(); selector_iter++) 
        {
            if (!selector_iter->second->isConnectedToSource())
                continue;
            
            if (!db_track)
                db_track = track_iter->second;
            
            track_config = selector_iter->second->sourceConfig->getTrackConfig(selector_iter->second->sourceTrackID);
            if (!track_config)
                throw ICException("Failed to find track %d in source config '%s'",
                                  selector_iter->second->sourceTrackID,
                                  selector_iter->second->sourceConfig->name.c_str());
            if (track_config->dataDef == PICTURE_DATA_DEFINITION) {
                db_track = track_iter->second;
                done = true;
            }
        }
    }
    
    if (!db_track)
        throw ICException("Failed to find audio or video track in multi-camera clip definition");
    
    return db_track;
}

static bool is_existing_cut(Database *db, MCTrackDef *db_track, XMCCut *x_cut)
{
    bool is_existing;
    vector<MCCut*> db_cuts = db->loadMultiCameraCuts(db_track, x_cut->Date(), x_cut->Position(),
                                                     x_cut->Date(), x_cut->Position() + 1);
    is_existing = !db_cuts.empty();
    
    vector<MCCut*>::iterator iter;
    for (iter = db_cuts.begin(); iter != db_cuts.end(); iter++)
        delete *iter;
    
    return is_existing;
}

static MCCut* create_db_cut(MCTrackDef *db_track, XMCCut *x_cut)
{
    auto_ptr<MCCut> db_cut(new MCCut());
    db_cut->mcTrackId = db_track->getDatabaseID();
    db_cut->mcSelectorIndex = x_cut->SelectorIndex();
    db_cut->cutDate = x_cut->Date();
    db_cut->position = x_cut->Position();
    db_cut->editRate = x_cut->EditRate();

    return db_cut.release();
}

static int save_cuts(Database *db, map<string, MCClipDef*> &db_clips, XMCFile *x_file)
{
    int cut_count = -1;
    MCClipDef *db_clip;
    MCTrackDef *db_track;
    vector<MCCut*> db_cuts;
    vector<MCCut*>::iterator db_cut_iter;
    
    try
    {
        // create database cuts
        vector<XMCCut*>& x_cuts = x_file->Cuts();
        vector<XMCCut*>::iterator x_cut_iter;
        for (x_cut_iter = x_cuts.begin(); x_cut_iter != x_cuts.end(); x_cut_iter++) {
            db_clip = get_db_clip(db_clips, (*x_cut_iter)->ClipName());
            
            db_track = get_assumed_db_track(db_clip);
            
            if (!is_existing_cut(db, db_track, *x_cut_iter))
                db_cuts.push_back(create_db_cut(db_track, *x_cut_iter));
        }
        

        // save cuts in a single transaction
        auto_ptr<Transaction> transaction(db->getTransaction("ImportCuts_SaveCuts"));
        
        for (db_cut_iter = db_cuts.begin(); db_cut_iter != db_cuts.end(); db_cut_iter++)
            db->saveMultiCameraCut(*db_cut_iter);

        transaction->commit();
        
        cut_count = db_cuts.size();
    }
    catch (const ProdAutoException &ex)
    {
        fprintf(stderr, "Error: %s\n", ex.getMessage().c_str());
    }
    catch (const ICException &ex)
    {
        fprintf(stderr, "Error: %s\n", ex.what());
    }
    catch (...)
    {
        fprintf(stderr, "Error:\n");
    }

    for (db_cut_iter = db_cuts.begin(); db_cut_iter != db_cuts.end(); db_cut_iter++)
        delete *db_cut_iter;
    
    return cut_count;
}

static void clear_stuff(map<long, std::string> &locations, map<string, SourceConfig*> &db_src_configs,
                        map<string, MCClipDef*> &db_clips)
{
    locations.clear();
    
    map<string, SourceConfig*>::iterator db_src_config_iter;
    for (db_src_config_iter = db_src_configs.begin(); db_src_config_iter != db_src_configs.end(); db_src_config_iter++)
        delete db_src_config_iter->second;
    db_src_configs.clear();
    
    map<string, MCClipDef*>::iterator db_clip_iter;
    for (db_clip_iter = db_clips.begin(); db_clip_iter != db_clips.end(); db_clip_iter++)
        delete db_clip_iter->second;
    db_clips.clear();
}



static void print_usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s --dbhost <host> --dbname <name> --dbuser <user> --dbpw <password> [files]\n", cmd);
}

int main(int argc, const char **argv)
{
    string dbhost = DEFAULT_DBHOST;
    string dbname = DEFAULT_DBNAME;
    string dbuser = DEFAULT_DBUSER;
    string dbpw = DEFAULT_DBPW;
    vector<string> file_list;
    int cmdln_index;

    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++) {
        if (strcmp(argv[cmdln_index], "-h") == 0 ||
            strcmp(argv[cmdln_index], "--help") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdln_index], "--dbhost") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbhost = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbname") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbname = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbuser") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbuser = argv[cmdln_index];
        }
        else if (strcmp(argv[cmdln_index], "--dbpw") == 0)
        {
            cmdln_index++;
            if (cmdln_index >= argc) {
                print_usage(argv[0]);
                return -1;
            }
            dbpw = argv[cmdln_index];
        }
        else
        {
            file_list.push_back(argv[cmdln_index]);
        }
    }

    Database *db;
    Database::initialise(dbhost, dbname, dbuser, dbpw, 2, 8);
    db = Database::getInstance();
    
    int error_count = 0;
    int imported_file_count = 0;
    int existing_file_count = 0;
    map<long, std::string> locations;
    map<string, SourceConfig*> src_configs;
    map<string, MCClipDef*> db_clips;

    vector<string>::const_iterator file_iter;
    for (file_iter = file_list.begin(); file_iter != file_list.end(); file_iter++) {
        try
        {
            printf("Importing '%s':\n", (*file_iter).c_str());
            
            XMCFile x_file;
            if (!x_file.Parse(*file_iter)) {
                error_count++;
                continue;
            }
            
            locations = import_or_load_locations(db, &x_file);
            src_configs = import_or_load_source_configs(db, locations, &x_file);
            db_clips = import_or_load_clip_defs(db, src_configs, &x_file);
            
            int cut_count = save_cuts(db, db_clips, &x_file);
            if (cut_count < 0) {
                // no need to print error because save_cuts() prints the error
                error_count++;
            } else {
                printf("%d of %zd cuts imported\n", cut_count, x_file.Cuts().size());
                if (cut_count == 0 && !x_file.Cuts().empty())
                    existing_file_count++;
                else
                    imported_file_count++;
            }
        }
        catch (const ProdAutoException &ex)
        {
            fprintf(stderr, "Error: %s\n", ex.getMessage().c_str());
            error_count++;
        }
        catch (const ICException &ex)
        {
            fprintf(stderr, "Error: %s\n", ex.what());
            error_count++;
        }
        catch (...)
        {
            fprintf(stderr, "Error:\n");
            error_count++;
        }
        
        clear_stuff(locations, src_configs, db_clips);
    }
    
    printf("\nResults:\n");
    printf("%zu files processed\n", file_list.size());
    printf("%d files imported\n", imported_file_count);
    printf("%d files were already imported\n", existing_file_count);
    printf("%d files resulted in an error\n", error_count);
    
    
    return 0;
}

