/*
 * $Id: test.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Utility to test the essence extraction code.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 
 
#include <stdio.h>
#include <stdlib.h>

#include <mxf_essence.h>


void usage(const char* cmd)
{
    fprintf(stderr, "%s <mxf filename>\n", cmd);
}

int main(int argv, const char* argc[])
{
    if (argv < 2)
    {
        usage(argc[0]);
        exit(1);
    }
    
    FILE* f;
    if ((f = fopen(argc[1], "rb")) == NULL)
    {
        fprintf(stderr, "Failed to open file %s\n", argc[1]);
        exit(1);
    }
    
    mxfe_EssenceType type;
    if (!mxfe_get_essence_type(f, &type))
    {
        fprintf(stderr, "Failed to get essence data type\n");
    }
    else
    {
        const char* suffix;
        if (!mxfe_get_essence_suffix(type, &suffix))
        {
            fprintf(stderr, "Failed to get essence data suffix\n");
        }
        else
        {
            printf("MXF essence type = '%s'\n", suffix);
            
            if (type != MXFE_UNKNOWN)
            {
                uint64_t offset;
                uint64_t len;
                if (!mxfe_get_essence_element_info(f, &offset, &len))
                {
                    fprintf(stderr, "Failed to get essence element offset and len\n");
                }
                else
                {
                    printf("Essence element offset = %lld and length = %lld\n", offset, len);
                }
            }
        }
    }
    
    fclose(f);
    
    return 0;
}
