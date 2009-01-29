/*
 * $Id: generate_http_resource.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>


int main(int argc, const char** argv)
{
    int isHTML;

    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s (--html|--binary) <filename>\n", argv[0]);
        return 1;
    }

    if (strcmp("--html", argv[1]) == 0)
    {
        isHTML = 1;
    }
    else if (strcmp("--binary", argv[1]) == 0)
    {
        isHTML = 0;
    }
    else
    {
        fprintf(stderr, "Usage: %s (--html|--binary) <filename>\n", argv[0]);
        return 1;
    }


    FILE* file;
    if ((file = fopen(argv[2], "rb")) == NULL)
    {
        perror("fopen");
        return 1;
    }

    if (isHTML)
    {
        printf("const char* g_xxxxxx = \n");
        printf("    \"");
    }
    else
    {
        printf("const unsigned char* g_xxxxxx = \n");
        printf("{\n");
    }

    int lineWidth = 80;
    if (!isHTML)
    {
        lineWidth = 20;
    }

    int size = 0;
    unsigned char buffer[80];
    int numRead = lineWidth;
    int i;
    int first = 1;
    while (numRead == lineWidth)
    {
        numRead = fread(buffer, 1, lineWidth, file);
        if (numRead > 0)
        {
            if (isHTML)
            {
                for (i = 0; i < numRead; i++)
                {
                    if (buffer[i] == '\"')
                    {
                        printf("\\%c", buffer[i]);
                    }
                    else if (buffer[i] == '\\')
                    {
                        printf("\\%c", buffer[i]);
                    }
                    else if (buffer[i] == '\n')
                    {
                        printf("\\n\"\n    \"", buffer[i]);
                    }
                    else
                    {
                        printf("%c", buffer[i]);
                    }
                }
            }
            else
            {
                size += numRead;
                if (!first)
                {
                    printf(",\n    ");
                }
                else
                {
                    printf("    ");
                    first = 0;
                }
                for (i = 0; i < numRead; i++)
                {
                    if (i != 0)
                    {
                        printf(",");
                    }
                    printf("0x%02x", buffer[i]);
                }
            }
        }
    }


    if (isHTML)
    {
        printf("\";\n");
    }
    else
    {
        printf("\n};\n");
    }

    fclose(file);

    return 0;
}
