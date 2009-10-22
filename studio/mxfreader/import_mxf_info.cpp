/*
 * $Id: import_mxf_info.cpp,v 1.2 2009/10/22 15:16:57 john_f Exp $
 *
 * Read MXF files and add metadata to database.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <Database.h>
#include <ProdAutoException.h>

#include "IngexMXFInfo.h"

using namespace std;


const char * const g_default_dbhost = "localhost";
const char * const g_default_dbname = "prodautodb";
const char * const g_default_dbuser = "bamzooki";
const char * const g_default_dbpw = "bamzooki";


static bool save_package(prodauto::Database *db, prodauto::Package *package)
{
    if (!package)
        return false;
 
    prodauto::Package *exist_package = db->loadPackage(package->uid, false);
    if (!exist_package) {
        // ensure the project name has been registered before saving the package
        if (!package->projectName.name.empty())
            package->projectName = db->loadOrCreateProjectName(package->projectName.name);

        // save the package
        try {
            db->savePackage(package);
        } catch (...) {
            // it might have been created by another process just after loadPackage above
            // check that it can now be loaded
            exist_package = db->loadPackage(package->uid, false);
            if (!exist_package)
                throw;
            
            delete exist_package;
        }
        
        return true;
    } else {
        delete exist_package;
        
        return false;
    }
}


static void print_usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s --dbhost <host> --dbname <name> --dbuser <user> --dbpw <password> [files]\n", cmd);
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv)
{
    string dbhost = g_default_dbhost;
    string dbname = g_default_dbname;
    string dbuser = g_default_dbuser;
    string dbpw = g_default_dbpw;
    vector<string> file_list;
    int cmdln_index;

    // parse the command line arguments
    for (cmdln_index = 1; cmdln_index < argc; cmdln_index++)
    {
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

    prodauto::Database *db;
    prodauto::Database::initialise(dbhost, dbname, dbuser, dbpw, 2, 8);
    db = prodauto::Database::getInstance();
    
    int file_count = 0;
    int error_count = 0;
    int saved_file_count = 0;
    int existing_file_count = 0;
    bool have_saved;

    vector<string>::const_iterator iter;
    for (iter = file_list.begin(); iter != file_list.end(); iter++)
    {
        file_count++;
        
        IngexMXFInfo *info;
        IngexMXFInfo::ReadResult result = IngexMXFInfo::read(*iter, &info);
        if (result != IngexMXFInfo::SUCCESS) {
            fprintf(stderr, "Error '%s': %s\n", (*iter).c_str(), IngexMXFInfo::errorToString(result).c_str());
            error_count++;
            continue;
        }
        

        try {

            have_saved = save_package(db, info->getPackageGroup()->GetMaterialPackage());
            have_saved = save_package(db, info->getPackageGroup()->GetFileSourcePackages().back()) || have_saved;
            have_saved = save_package(db, info->getPackageGroup()->GetTapeSourcePackage()) || have_saved;
            
            if (have_saved)
                saved_file_count++;
            else
                existing_file_count++;
            
        } catch (const prodauto::ProdAutoException &ex) {
            fprintf(stderr, "Error: failed to register metadata with database: %s\n", ex.getMessage().c_str());
            error_count++;
        } catch (...) {
            fprintf(stderr, "Error: failed to register metadata with database\n");
            error_count++;
        }

        
        delete info;
    }
    
    printf("%d files processed\n", file_count);
    printf("%d files saved\n", saved_file_count);
    printf("%d files were already saved\n", existing_file_count);
    printf("%d files resulted in an error\n", error_count);
    
    
    return 0;
}

