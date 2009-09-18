// Copyright (C) 2009  British Broadcasting Corporation
// Author: Sean Casey <seantc@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Database.h>
#include <DBException.h>
#include <DatabaseEnums.h>

using namespace std;
using namespace prodauto;

int main(int argc, char* argv[])
{
	bool debug = false;
	char *inStr = argv[1];
	long pkg_identifier;
	long packages[100000];


	// Delete packages from database
	try
	{
		// Initialise
		prodauto::Database::initialise("localhost", "prodautodb", "bamzooki", "bamzooki", 4, 12);

		Database* db = Database::getInstance();
		Transaction* transaction = (db->getTransaction("DeletePackages"));

		// String to array
		char *nextTok = strtok(inStr, ",");
		int i=0;

		while(nextTok != NULL){
			packages[i] = atol(nextTok);
			i++;
			nextTok = strtok(NULL, ",");
		}

		int noPackages = i;

		if(noPackages == 0){
			printf("Error! No package IDs supplied.\n");
			return 1;
		}

		for (i = 0; i < noPackages; i++){
			try
			{
				pkg_identifier = packages[i];
				if(debug){printf("Loading package: %li\n", pkg_identifier);}
				Package* package = (db->loadPackage(pkg_identifier));
				db->deletePackageChain(package, transaction);
				transaction->commit();
				if(debug){printf("Deleted package\n");}
			}
			catch (const prodauto::DBException & dbe)
			{
				printf("Error deleting packages\n");
				return 1;
			}
		}

		printf("Deleted %i material item(s)\n", noPackages);
		return 0;

		//TODO: Check for unreferenced isolated packages
	}
	catch (const prodauto::DBException & dbe)
	{
		printf("Error\n");
		return 1;
	}
}
