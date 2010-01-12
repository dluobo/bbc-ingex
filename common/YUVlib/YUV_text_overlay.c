/*
 * $Id: YUV_text_overlay.c,v 1.4 2010/01/12 16:10:20 john_f Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Jim Easterbrook
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

/*
    Note that some functions have an _player version.
    It would be nice to merge to a common version sometime.
*/

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <fontconfig.h>
#include <unistd.h>

#include "YUV_frame.h"
#include "YUV_text_overlay.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// This struct stores a bunch of "static" data used by the text_overlay
// routines.
typedef struct info_rec
{
    // font config
    FcConfig*	config;
    // free type
    FT_Face	face;
    FT_Library	library;
    char	current_family[128];
} info_rec;

void free_info_rec(p_info_rec* p_info)
{
    info_rec*	info;
    
    if (*p_info == NULL)
        return;
    info = *p_info;
    if (info->face != NULL)
    {
        FT_Done_Face(info->face);
        info->face = NULL;
    }
    FT_Done_FreeType(info->library);
    if (info->config != NULL)
    {
        FcConfigDestroy(info->config);
        info->config = NULL;
    }
    free(info);
    *p_info = NULL;
}

static int allocate_info(p_info_rec* p_info)
{
    info_rec*	info;

    *p_info = malloc(sizeof(info_rec));
    if (*p_info == NULL)
        return YUV_no_memory;
    info = *p_info;
    info->face = NULL;
    strcpy(info->current_family, "");
    // initialise FontConfig
    info->config = FcInitLoadConfigAndFonts();
    if (info->config == NULL)
        return YUV_fontconfig;
    // initialise FreeType
    if (FT_Init_FreeType(&info->library))
        return YUV_freetype;
    return YUV_OK;
}

static int find_font(info_rec* info, char* family, char* path)
{
    FcPattern*	pattern;
    FcPattern*	matchedPattern;
    FcResult	result;
    FcChar8*	s;

    pattern = FcPatternBuild(NULL, FC_FAMILY, FcTypeString, family,
                             (char*) NULL);
    if (pattern == NULL)
        return YUV_fontconfig;
    if (FcConfigSubstitute(info->config, pattern, FcMatchPattern) != FcTrue)
        return YUV_fontconfig;
    FcDefaultSubstitute(pattern);
    matchedPattern = FcFontMatch(info->config, pattern, &result);
    FcPatternDestroy(pattern);
    if (FcPatternGetString(matchedPattern, FC_FILE, 0, &s) != FcResultMatch)
        return YUV_fontconfig;
    strcpy(path, (char*)s);
    FcPatternDestroy(matchedPattern);
    return YUV_OK;
}

static int set_font(info_rec* info, char* family, int size,
                    int aspect_ratio_num, int aspect_ratio_den)
{
    char	fontPath[1024];
    int		width, height;
    int		result;

    if (strcmp(family, info->current_family) != 0)
    {
        // free the current face
        if (info->face != NULL)
        {
            FT_Done_Face(info->face);
            info->face = NULL;
        }
        // need to find and load font face
        result = find_font(info, family, fontPath);
        if (result < 0)
            return result;
        // load the font into face
        if (FT_New_Face(info->library, fontPath, 0, &info->face))
            return YUV_freetype;
        // attempt to load kerning information
        {
            char*		dot;

            dot = strrchr(fontPath, '.');
            *dot = '\0';
            strcat(fontPath, ".afm");
            if (access(fontPath, R_OK) == 0)
                FT_Attach_File(info->face, fontPath);
        }
        strcpy(info->current_family, family);
    }
    // set char size
    height = size * 64 * 72;
    width = (height * aspect_ratio_den * 702) / (aspect_ratio_num * 575);
    if (FT_Set_Char_Size(info->face, width, height, 1, 1))
        return YUV_freetype;
    return YUV_OK;
}

void free_overlay(overlay* ovly)
{
    if (ovly != NULL && ovly->buff != NULL)
    {
        free(ovly->buff);
        ovly->buff = NULL;
    }
}

static void filterUV(overlay* ovly, const int ssx, const int ssy)
{
    BYTE*	srcLineA;
    BYTE*	srcLineB;
    BYTE*	dstLine;
    int		i, j;

    // make UV image by filtering Y image
    ovly->Cbuff = ovly->buff + (ovly->w * ovly->h);
    // horizontal
    if (ssx == 1)
    {
        // copy data
        memcpy(ovly->Cbuff, ovly->buff, ovly->w * ovly->h);
    }
    else if (ssx == 2)
    {
        int	A, B, C;

        // 1/4, 1/2, 1/4 filter
        srcLineA = ovly->buff;
        dstLine = ovly->Cbuff;
        for (j = 0; j < ovly->h; j++)
        {
            A = 0;
            B = *srcLineA++;
            for (i = 0; i < ovly->w - 1; i++)
            {
                C = *srcLineA++;
                *dstLine++ = (A + (B * 2) + C) / 4;
                A = B;
                B = C;
            }
            *dstLine++ = (A + (B * 2)) / 4;
        }
    }
    // vertical
    if (ssy == 2)
    {
        // 1/2, 1/2 filter
        srcLineA = ovly->Cbuff;
        srcLineB = ovly->Cbuff + ovly->w;
        dstLine = ovly->Cbuff;
        for (j = 0; j < ovly->h - 1; j++)
        {
            for (i = 0; i < ovly->w; i++)
                *dstLine++ = (*srcLineA++ + *srcLineB++) / 2;
        }
        for (i = 0; i < ovly->w; i++)
            *dstLine++ = *srcLineA++ / 2;
    }
    ovly->ssx = ssx;
    ovly->ssy = ssy;
}

int text_to_overlay(p_info_rec* p_info, overlay* ovly, char* text,
                    int max_width, char* font, const int size, 
                    const int aspect_ratio_num,
                    const int aspect_ratio_den)
{
    info_rec*	info;
    BYTE*	srcLine;
    BYTE*	dstLine;
    int		j;
    int		error;
    int		result;

    // initialise our data
    if (*p_info == NULL)
    {
        result = allocate_info(p_info);
        if (result < 0)
            return result;
    }
    info = *p_info;
    result = set_font(info, font, size, aspect_ratio_num, aspect_ratio_den);
    if (result < 0)
        return result;
    // render text using freetype2
    {
        #define MAX_GLYPHS 100
        FT_GlyphSlot	slot = info->face->glyph;  /* a small shortcut */
        FT_UInt		glyph_idx;
        FT_UInt		last_glyph;
        FT_Vector	delta;
        FT_Bool		use_kerning;
        int		pen_x, pen_y, n, margin;
        FT_Glyph	glyphs[MAX_GLYPHS];   /* glyph image    */    
        FT_Vector	pos   [MAX_GLYPHS];   /* glyph position */    
        FT_UInt		num_glyphs;
        int		vis_last;	// index of end of last "word"
        int		vis_width;	// width to vis_last

        use_kerning = FT_HAS_KERNING(info->face);
        // font metrics are unreliable, so use actual rendered characters
        // set left margin using a capital M character
        glyph_idx = FT_Get_Char_Index(info->face, 'M');
        FT_Load_Glyph(info->face, glyph_idx, FT_LOAD_DEFAULT);
        FT_Get_Glyph(info->face->glyph, &glyphs[0]);
        pen_x = slot->metrics.horiBearingX;
        margin = pen_x;
        FT_Done_Glyph(glyphs[0]);
        // set baseline using a capital A character
        glyph_idx = FT_Get_Char_Index(info->face, 'A');
        FT_Load_Glyph(info->face, glyph_idx, FT_LOAD_DEFAULT);
        FT_Get_Glyph(info->face->glyph, &glyphs[0]);
        pen_y = slot->metrics.horiBearingY;
        FT_Done_Glyph(glyphs[0]);
        // convert each character to a glyph and set its position
        num_glyphs = 0;
        vis_last = -1;
        vis_width = 0;
        last_glyph = FT_Get_Char_Index(info->face, ' ');
        for (n = 0; n < (int)strlen(text); n++)
        {
            if (text[n] == '\n')
                break;
            if (num_glyphs >= MAX_GLYPHS)
                break;
            glyph_idx = FT_Get_Char_Index(info->face, text[n]);
            if (use_kerning && last_glyph && glyph_idx)
            {
                FT_Get_Kerning(info->face, last_glyph, glyph_idx,
                               FT_KERNING_DEFAULT, &delta);
                pen_x += delta.x;
            }
            error = FT_Load_Glyph(info->face, glyph_idx, FT_LOAD_DEFAULT);
            if (error)
                continue;  /* ignore errors */
            error = FT_Get_Glyph(info->face->glyph, &glyphs[num_glyphs]);
            if (error)
                continue;  /* ignore errors */
            if (pen_y - slot->metrics.horiBearingY < 0)
            {
                fprintf(stderr, "Character '%c' ascends too high\n", text[n]);
                pen_y = slot->metrics.horiBearingY;
            }
            if (pen_y - slot->metrics.horiBearingY +
                        slot->metrics.height > size * 64)
            {
                fprintf(stderr, "Character '%c' descends too low\n", text[n]);
                exit(1);
            }
            pos[num_glyphs].x = pen_x;
            pos[num_glyphs].y = -pen_y;

            pen_x += slot->advance.x;
            last_glyph = glyph_idx;
            num_glyphs++;

            if (pen_x + (margin * 2) > max_width * 64)
                break;

            if (text[n] != ' ' &&
               (text[n+1] == ' ' || text[n+1] == '\0' || text[n+1] == '\n'))
            {
                vis_last = n;
                vis_width = pen_x;
            }
        }
        // truncate string to end of last word
        if (vis_last < 0)	// haven't found end of first word!
        {
            vis_last = num_glyphs - 1;
            vis_width = pen_x;
        }
        num_glyphs = vis_last + 1;
        // find start of next word
        result = num_glyphs;
        while (text[result] == ' ' || text[result] == '\n')
            result++;
        // set overlay dimensions
        ovly->w = (vis_width + (margin * 2)) / 64;
        ovly->h = size;
//        fprintf(stderr, "Area of '%s' = (%d x %d)\n", text, ovly->w, ovly->h);
        ovly->ssx = -1;
        ovly->ssy = -1;
        // alloc memory
        ovly->buff = malloc(ovly->w * ovly->h * 2);
        if (ovly->buff == NULL)
            return YUV_no_memory;
        memset(ovly->buff, 0, ovly->w * ovly->h * 2);
        ovly->Cbuff = NULL;
        // render glyphs in pre-calculated positions
        for (n = 0; n < (int)num_glyphs; n++)
        {
            error = FT_Glyph_To_Bitmap(&glyphs[n], FT_RENDER_MODE_NORMAL,
                                       &pos[n], 1);
            if (!error)
            {
                FT_BitmapGlyph	bit = (FT_BitmapGlyph)glyphs[n];
                srcLine = bit->bitmap.buffer;
                dstLine = ovly->buff - (bit->top * ovly->w) + bit->left;
                for (j = 0; j < bit->bitmap.rows; j++)
                {
                    memcpy(dstLine, srcLine, bit->bitmap.width);
                    srcLine += bit->bitmap.width;
                    dstLine += ovly->w;
                }
            }
            FT_Done_Glyph(glyphs[n]);
        }
    }
    return result;	// length of string actually rendered
}

YUV_error text_to_overlay_player(p_info_rec* p_info, overlay* ovly, char* text,
                          int max_width, int min_width,
                          int x_margin, int y_margin,
                          int center,
                          int tab_width,
                          int enable_align_right,
                          char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den)
{
    info_rec*   info;
    BYTE*   srcLine;
    BYTE*   dstLine;
    int     j;
    int     error;
    int     result;

    // initialise our data
    if (*p_info == NULL)
    {
        result = allocate_info(p_info);
        if (result < 0)
            return result;
    }
    info = *p_info;
    result = set_font(info, font, size, aspect_ratio_num, aspect_ratio_den);
    if (result < 0)
        return result;
    // render text using freetype2
    {
        #define MAX_GLYPHS 100
        FT_GlyphSlot    slot = info->face->glyph;  /* a small shortcut */
        FT_UInt     glyph_idx;
        FT_UInt     last_glyph;
        FT_Vector   delta;
        FT_Bool     use_kerning;
        int     pen_x, pen_y, n, margin;
        FT_Glyph    glyphs[MAX_GLYPHS];   /* glyph image    */
        FT_Vector   pos   [MAX_GLYPHS];   /* glyph position */
        FT_UInt     num_glyphs;
        FT_UInt     render_num_glyphs;
        FT_UInt     vis_last_glyph;
        int     vis_last;   // index of end of last "word"
        int     vis_width;  // width to vis_last
        int     x_offset = 0;
        int     align_right = 0;
        int     align_right_glyph = 0;
        int     align_right_shift;

        use_kerning = FT_HAS_KERNING(info->face);
        // compute initial pen position using font metrics
        n = info->face->bbox.yMax - info->face->bbox.yMin;//info->face->ascender - info->face->descender;
        pen_x = ((-info->face->bbox.xMin * size) + (n / 2)) / n;
        pen_y = ((info->face->bbox.yMax * size) + (n / 2)) / n;
        margin = pen_x;
        // convert each character to a glyph and set its position
        num_glyphs = 0;
        vis_last = -1;
        vis_last_glyph = 0;
        vis_width = 0;
        last_glyph = FT_Get_Char_Index(info->face, ' ');
        for (n = 0; n < (int)strlen(text); n++)
        {
            if (text[n] == '\n')
                break;
            if (num_glyphs >= MAX_GLYPHS)
                break;
            if (tab_width > 0 && text[n] == '\t')
            {
                pen_x += size - (pen_x % size);
            }
            // a '>>' sequence results in alignment of the following text to the right
            else if (enable_align_right && !align_right && text[n] == '>' && text[n + 1] == '>')
            {
                n += 1;
                align_right = 1;
                align_right_glyph = num_glyphs;
                continue;
            }
            else
            {
                glyph_idx = FT_Get_Char_Index(info->face, text[n]);
                if (use_kerning && last_glyph && glyph_idx)
                {
                    FT_Get_Kerning(info->face, last_glyph, glyph_idx,
                                   FT_KERNING_DEFAULT, &delta);
                    pen_x += delta.x / 64;
                }
                error = FT_Load_Glyph(info->face, glyph_idx, FT_LOAD_DEFAULT);
                if (error)
                    continue;  /* ignore errors */
                error = FT_Get_Glyph(info->face->glyph, &glyphs[num_glyphs]);
                if (error)
                    continue;  /* ignore errors */
                if (pen_y - slot->bitmap_top < 0)
                {
                    fprintf(stderr, "Character '%c' ascends too high\n", text[n]);
                    pen_y = slot->bitmap_top;
                }
                if (pen_y - slot->bitmap_top + slot->bitmap.rows > size)
                {
                    fprintf(stderr, "Character '%c' descends too low\n", text[n]);
                    exit(1);
                }
                pos[num_glyphs].x = pen_x;
                pos[num_glyphs].y = pen_y;

                pen_x += slot->advance.x / 64;
                last_glyph = glyph_idx;
                num_glyphs++;
            }

            if (pen_x + (margin * 2) + (x_margin * 2) > max_width)
                break;

            if (text[n] != ' ' &&
               (text[n+1] == ' ' || text[n+1] == '\0' || text[n+1] == '\n'))
            {
                vis_last_glyph = num_glyphs;
                vis_last = n;
                vis_width = pen_x;
            }
        }
        // truncate string to end of last word
        if (vis_last < 0)   // haven't found end of first word!
        {
            vis_last_glyph = num_glyphs;
            vis_last = n - 1;
            vis_width = pen_x;
        }
        if (vis_width + (margin * 2) + (x_margin * 2) > max_width)
        {
            vis_last_glyph--;
            vis_last--;
            vis_width -= slot->advance.x / 64;
        }
        render_num_glyphs = vis_last_glyph;
        result = vis_last + 1;
        // do right alignment if there is space
        if (align_right)
        {
            align_right_shift = max_width - (vis_width + (margin * 2) + (x_margin * 2));
            if (align_right_shift > 0)
            {
                for (n = align_right_glyph; n < (int)render_num_glyphs; n++)
                {
                    pos[n].x += align_right_shift;
                }
            }
        }
        // find start of next word
        while (text[result] == ' ' || text[result] == '\n')
            result++;
        // set overlay dimensions
        ovly->w = vis_width + (margin * 2) + (x_margin * 2);
        // set width >= min_width and center
        if (ovly->w < min_width && min_width <= max_width)
        {
            if (center)
            {
                x_offset = (min_width - ovly->w) / 2;
            }
            ovly->w = min_width;
        }
        ovly->h = size + 2 * y_margin;
//        fprintf(stderr, "Area of '%s' = (%d x %d)\n", text, ovly->w, ovly->h);
        ovly->ssx = -1;
        ovly->ssy = -1;
        // alloc memory
        ovly->buff = malloc(ovly->w * ovly->h * 2);
        if (ovly->buff == NULL)
            return YUV_no_memory;
        memset(ovly->buff, 0, ovly->w * ovly->h * 2);
        ovly->Cbuff = NULL;
        // render glyphs
        for (n = 0; n < (int)render_num_glyphs; n++)
        {
            error = FT_Glyph_To_Bitmap(&glyphs[n], FT_RENDER_MODE_NORMAL,
                                       0, 1);
            if (!error)
            {
                FT_BitmapGlyph  bit = (FT_BitmapGlyph)glyphs[n];
                srcLine = bit->bitmap.buffer;
                dstLine = ovly->buff + ((pos[n].y - bit->top) * ovly->w) +
                                        pos[n].x + bit->left +
                                        x_margin +
                                        y_margin * ovly->w +
                                        x_offset;
                // TODO: fix the problem with offsets
                if (dstLine < ovly->buff)
                    dstLine = ovly->buff;
                for (j = 0; j < bit->bitmap.rows; j++)
                {
                    memcpy(dstLine, srcLine, bit->bitmap.width);
                    srcLine += bit->bitmap.width;
                    dstLine += ovly->w;
                }
            }
        }
        // cleanup glyphs
        for (n = 0; n < (int)num_glyphs; n++)
        {
            FT_Done_Glyph(glyphs[n]);
        }
    }
    return result;  // length of string actually rendered
}


int ml_text_to_ovly(p_info_rec* info, overlay* ovly, char* text,
                    int max_width, char* font, const int size,
                    const int aspect_ratio_num, const int aspect_ratio_den)
{
    #define MAX_LINES 100
    overlay	line_ovly[MAX_LINES];
    char*	sub_str;
    int		count;
    int		length;
    int		no_lines;
    BYTE*	src;
    BYTE*	dst;
    int		j, n;

    // render each line of text to an overlay
    sub_str = text;
    length = strlen(text);
    no_lines = 0;
    while (length > 0 && no_lines < MAX_LINES)
    {
        count = text_to_overlay(info, &line_ovly[no_lines], sub_str, max_width,
                                font, size, aspect_ratio_num, aspect_ratio_den);
        if (count < 0)
            return count;
        sub_str += count;
        length -= count;
        no_lines += 1;
    }
    // create overlay to accomodate every rendered line
    ovly->h = no_lines * size;
    ovly->w = 0;
    for (n = 0; n < no_lines; n++)
        ovly->w = max(ovly->w, line_ovly[n].w);
    ovly->ssx = -1;
    ovly->ssy = -1;
    ovly->buff = malloc(ovly->w * ovly->h * 2);
    if (ovly->buff == NULL)
        return YUV_no_memory;
    memset(ovly->buff, 0, ovly->w * ovly->h * 2);
    ovly->Cbuff = NULL;
    // copy rendered text
    for (n = 0; n < no_lines; n++)
    {
        src = line_ovly[n].buff;
        dst = ovly->buff + (ovly->w * n * size);
        for (j = 0; j < line_ovly[n].h; j++)
        {
            memcpy(dst, src, line_ovly[n].w);
            src += line_ovly[n].w;
            dst += ovly->w;
        }
        free_overlay(&line_ovly[n]);
    }
    return YUV_OK;
}

YUV_error ml_text_to_ovly_player(p_info_rec* info, overlay* ovly, char* text,
                          int max_width, char* font, const int size, int margin,
                          const int aspect_ratio_num, const int aspect_ratio_den)
{
    #define MAX_LINES 100
    overlay line_ovly[MAX_LINES];
    char*   sub_str;
    int     count;
    int     length;
    int     no_lines;
    BYTE*   src;
    BYTE*   dst;
    int     j, n;

    // render each line of text to an overlay
    sub_str = text;
    length = strlen(text);
    no_lines = 0;
    while (length > 0 && no_lines < MAX_LINES)
    {
        count = text_to_overlay_player(info, &line_ovly[no_lines], sub_str, max_width, 0,
                                0, 0, 0, 0, 0, font, size, aspect_ratio_num, aspect_ratio_den);
        if (count < 0)
            return count;
        sub_str += count;
        length -= count;
        no_lines += 1;
    }
    // create overlay to accomodate every rendered line
    ovly->h = no_lines * size;
    ovly->w = 0;
    for (n = 0; n < no_lines; n++)
        ovly->w = max(ovly->w, line_ovly[n].w);
    ovly->h += margin * 2;
    ovly->w += margin * 2;
    ovly->ssx = -1;
    ovly->ssy = -1;
    ovly->buff = malloc(ovly->w * ovly->h * 2);
    if (ovly->buff == NULL)
        return YUV_no_memory;
    memset(ovly->buff, 0, ovly->w * ovly->h * 2);
    ovly->Cbuff = NULL;
    // copy rendered text
    for (n = 0; n < no_lines; n++)
    {
        src = line_ovly[n].buff;
        dst = ovly->buff + (ovly->w * n * size) + margin * ovly->w + margin;
        for (j = 0; j < line_ovly[n].h; j++)
        {
            memcpy(dst, src, line_ovly[n].w);
            src += line_ovly[n].w;
            dst += ovly->w;
        }
        free_overlay(&line_ovly[n]);
    }
    return YUV_OK;
}


int text_to_4box(p_info_rec* info, overlay* ovly,
                 char* txt_0, char* txt_1, char* txt_2, char* txt_3,
                 int max_width, char* font, const int size,
                 const int aspect_ratio_num, const int aspect_ratio_den)
{
    int		n, w, h, j, x, y;
    BYTE*	ptr;
    BYTE*	src;
    BYTE*	dst;
    overlay	txt_ovly[4];
    char*	title[] = {txt_0, txt_1, txt_2, txt_3};
    int		result;

    // render the four text strings
    for (n = 0; n < 4; n++)
    {
        result = text_to_overlay(info, &txt_ovly[n], title[n],
                                 (max_width / 2) - 3, font, size,
                                 aspect_ratio_num, aspect_ratio_den);
        if (result < 0)
            return result;
    }
    // get dimensions of biggest string
    w = 0;
    h = 0;
    for (n = 0; n < 4; n++)
    {
        if (w < txt_ovly[n].w)
            w = txt_ovly[n].w;
        if (h < txt_ovly[n].h)
            h = txt_ovly[n].h;
    }
    // expand box a little
    w += 0;
    h += 8;
    // init result ovly
    ovly->h = (h * 2) + 2;
    ovly->w = (w * 2) + 2;
    ovly->ssx = -1;
    ovly->ssy = -1;
    ovly->buff = malloc(ovly->w * ovly->h * 2);
    if (ovly->buff == NULL)
        return YUV_no_memory;
    memset(ovly->buff, 0, ovly->w * ovly->h * 2);
    ovly->Cbuff = NULL;
    // copy rendered text
    for (n = 0; n < 4; n++)
    {
        x = (w + 2 - txt_ovly[n].w) / 2;
        y = 7;
        x += (n % 2) * w;
        y += (n / 2) * h;
        src = txt_ovly[n].buff;
        dst = ovly->buff + (ovly->w * y) + x;
        for (j = 0; j < txt_ovly[n].h; j++)
        {
            memcpy(dst, src, txt_ovly[n].w);
            src += txt_ovly[n].w;
            dst += ovly->w;
        }
        free_overlay(&txt_ovly[n]);
    }
    // draw horizontal grid lines
    for (y = 0; y < ovly->h; y += h)
    {
        ptr = ovly->buff + (ovly->w * y);
        for (x = 0; x < ovly->w; x++)
            *ptr++ = 160;
        ptr = ovly->buff + (ovly->w * (y + 1));
        for (x = 0; x < ovly->w; x++)
            *ptr++ = 160;
    }
    // draw vertical grid lines
    for (x = 0; x < ovly->w; x += w)
    {
        ptr = ovly->buff + x;
        for (y = 0; y < ovly->h; y++)
        {
            *ptr = 160;
            ptr += ovly->w;
        }
        ptr = ovly->buff + x + 1;
        for (y = 0; y < ovly->h; y++)
        {
            *ptr = 160;
            ptr += ovly->w;
        }
    }
    return YUV_OK;
}

int add_overlay(overlay* ovly, YUV_frame* frame, int x, int y,
                BYTE txtY, BYTE txtU, BYTE txtV, int box)
{
    int		i, j;
    int		x0, x1, y0, y1;
    int		ssx, ssy;
    int		box_key;
    int		txt_key;
    int		bg;
    BYTE*	srcLine;
    BYTE*	dstLine;
    BYTE*	dstLine2;
    BYTE*	srcPtr;
    BYTE*	dstPtr;
    BYTE*	dstPtr2;

    // set clipped start and end points
    x0 = max(x, 0);
    y0 = max(y, 0);
    x1 = min(x + ovly->w, frame->Y.w);
    y1 = min(y + ovly->h, frame->Y.h);
    if ((x0 >= x1) || (y0 >= y1))
        return YUV_OK;
    // check chroma subsampling
    ssx = frame->Y.w / frame->U.w;
    ssy = frame->Y.h / frame->U.h;
    if (ovly->Cbuff == NULL || ovly->ssx != ssx || ovly->ssy != ssy)
        // make UV image by filtering Y image
        filterUV(ovly, ssx, ssy);
    // normalise colour
    box_key = 256 - (256 * box / 100);
    // do Y
    srcLine = ovly->buff + ((y0 - y) * ovly->w) + (x0 - x);
    dstLine = frame->Y.buff + (y0 * frame->Y.lineStride) +
                              (x0 * frame->Y.pixelStride);
    for (j = y0; j < y1; j++)
    {
        srcPtr = srcLine;
        dstPtr = dstLine;
        for (i = x0; i < x1; i++)
        {
            // dim background level over entire box
            bg = 16 + ((box_key * (*dstPtr - 16)) / 256);
            // key in foreground level
            *dstPtr = bg + ((*srcPtr++ * (txtY - bg)) / 256);
            // next pixel
            dstPtr += frame->Y.pixelStride;
        }
        srcLine += ovly->w;
        dstLine += frame->Y.lineStride;
    }
    // do UV
    // adjust start position to cosited YUV samples
    i = x0 % ssx;
    if (i != 0)
        x0 += ssx - i;
    j = y0 % ssy;
    if (j != 0)
        y0 += ssy - j;
    srcLine = ovly->Cbuff + ((y0 - y) * ovly->w) + x0 - x;
    dstLine = frame->U.buff + ((y0/ssy) * frame->U.lineStride) +
                              ((x0/ssx) * frame->U.pixelStride);
    dstLine2 = frame->V.buff + ((y0/ssy) * frame->V.lineStride) +
                               ((x0/ssx) * frame->V.pixelStride);
    for (j = y0; j < y1; j += ssy)
    {
        srcPtr = srcLine;
        dstPtr = dstLine;
        dstPtr2 = dstLine2;
        for (i = x0; i < x1; i += ssx)
        {
            txt_key = *srcPtr;
            srcPtr += ssx;
            // dim background level over entire box
            bg = 128 + ((box_key * (*dstPtr - 128)) / 256);
            // key in foreground level
            *dstPtr = bg + ((txt_key * (txtU - bg)) / 256);
            dstPtr += frame->U.pixelStride;
            // and again for V
            bg = 128 + ((box_key * (*dstPtr2 - 128)) / 256);
            *dstPtr2 = bg + ((txt_key * (txtV - bg)) / 256);
            dstPtr2 += frame->V.pixelStride;
        }
        srcLine += (ssy * ovly->w);
        dstLine += frame->U.lineStride;
        dstLine2 += frame->V.lineStride;
    }
    return YUV_OK;
}

void free_timecode(timecode_data* tc_data)
{
    int	c;

    for (c = 0; c < 11; c++)
        free_overlay(&tc_data->tc_ovly[c]);
}

int init_timecode(p_info_rec* p_info, timecode_data* tc_data,
                  char* font, const int size,
                  const int aspect_ratio_num, const int aspect_ratio_den)
{
    info_rec*		info;
    FT_GlyphSlot	slot;
    char		cset[] = "0123456789:";
    int         bb_t, bb_b, bb_l, bb_r; // bounding box
    BYTE*		dstLine;
    BYTE*		srcPtr;
    BYTE*		dstPtr;
    int			c, j;
    int			result;

    // initialise our data
    if (*p_info == NULL)
    {
        result = allocate_info(p_info);
        if (result < 0)
            return result;
    }
    info = *p_info;
    // create a suitable text image
    result = set_font(info, font, size, aspect_ratio_num, aspect_ratio_den);
    if (result < 0)
        return result;
    // get bounding box for characters
    bb_t =  1000000;
    bb_b = -1000000;
    bb_l =  1000000;
    bb_r = -1000000;
    for (c = 0; c < 11; c++)
    {
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(info->face, cset[c], FT_LOAD_RENDER))
            return YUV_freetype;
        slot = info->face->glyph;  /* a small shortcut */
        if (bb_t > -slot->bitmap_top)
            bb_t = -slot->bitmap_top;
        if (bb_b < slot->bitmap.rows - slot->bitmap_top)
            bb_b = slot->bitmap.rows - slot->bitmap_top;
        if (bb_l > slot->bitmap_left)
            bb_l = slot->bitmap_left;
        if (bb_r < slot->bitmap_left + slot->bitmap.width)
            bb_r = slot->bitmap_left + slot->bitmap.width;
    }
    // expand bounding box a little
    bb_t -= 1;
    bb_b += 1;
    bb_l -= 1;
    bb_r += 1;
    tc_data->height = bb_b - bb_t;
    // initialise character overlays
    for (c = 0; c < 11; c++)
    {
        tc_data->tc_ovly[c].w = bb_r - bb_l;
        tc_data->tc_ovly[c].h = tc_data->height;
        tc_data->tc_ovly[c].ssx = -1;
        tc_data->tc_ovly[c].ssy = -1;
        tc_data->tc_ovly[c].buff = malloc(tc_data->tc_ovly[c].w *
                                          tc_data->tc_ovly[c].h * 2);
        if (tc_data->tc_ovly[c].buff == NULL)
            return YUV_no_memory;
        memset(tc_data->tc_ovly[c].buff, 0, tc_data->tc_ovly[c].w *
                                            tc_data->tc_ovly[c].h * 2);
        tc_data->tc_ovly[c].Cbuff = NULL;
    }
    // copy bitmaps
    for (c = 0; c < 11; c++)
    {
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(info->face, cset[c], FT_LOAD_RENDER))
            return YUV_freetype;
        slot = info->face->glyph;  /* a small shortcut */
        if (c == 10)
        {
            // make colon narrower than other characters
            tc_data->tc_ovly[c].w = slot->advance.x / 64;
        }
        srcPtr = slot->bitmap.buffer;
        dstLine = tc_data->tc_ovly[c].buff;
        // add vertical offset
        dstLine += tc_data->tc_ovly[c].w * (-slot->bitmap_top - bb_t);
        // horizontally centre character
        dstLine += (tc_data->tc_ovly[c].w - slot->bitmap.width) / 2;
        for (j = 0; j < slot->bitmap.rows; j++)
        {
            dstPtr = dstLine;
            memcpy(dstLine, srcPtr, slot->bitmap.width);
            srcPtr += slot->bitmap.width;
            dstLine += tc_data->tc_ovly[c].w;
        }
    }
    tc_data->width = (tc_data->tc_ovly[0].w * 8) +
                     (tc_data->tc_ovly[10].w * 3);	// 8 digits and 3 colons
    return YUV_OK;
}

int add_timecode(timecode_data* tc_data, const int frameNo,
                 YUV_frame* frame, int x, int y,
                 BYTE txtY, BYTE txtU, BYTE txtV, int box)
{
    int		c, n;
    int		offset;
    int		hr, mn, sc, fr;
    char	tc_str[16];
    int		result;

    // convert frame no to time code string
    hr = frameNo;
    fr = hr % 25;	hr = hr / 25;
    sc = hr % 60;	hr = hr / 60;
    mn = hr % 60;	hr = hr / 60;
    sprintf(tc_str, "%02d:%02d:%02d:%02d", hr, mn, sc, fr);
    // legitimise start point
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    offset = 0;
    for (n = 0; n < 11; n++)
    {
        c = tc_str[n] - '0';
        result = add_overlay(&tc_data->tc_ovly[c], frame, x + offset, y,
                             txtY, txtU, txtV, box);
        if (result < 0)
            return result;
        offset += tc_data->tc_ovly[c].w;
    }
    return YUV_OK;
}

YUV_error add_timecode_player(timecode_data* tc_data, int hr, int mn, int sc, int fr, int isPAL,
                       YUV_frame* frame, int x, int y,
                       BYTE txtY, BYTE txtU, BYTE txtV, int box)
{
    int     c, n;
    int     offset;
    char    tc_str[16];
    int     result;

    if (isPAL)
    {
        sprintf(tc_str, "%02d:%02d:%02d:%02d", hr, mn, sc, fr);
    }
    else
    {
        sprintf(tc_str, "%02d;%02d;%02d;%02d", hr, mn, sc, fr);
    }
    // legitimise start point
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    offset = 0;
    for (n = 0; n < 11; n++)
    {
        c = tc_str[n] - '0';
        result = add_overlay(&tc_data->tc_ovly[c], frame, x + offset, y,
                             txtY, txtU, txtV, box);
        if (result < 0)
            return result;
        offset += tc_data->tc_ovly[c].w;
    }
    return YUV_OK;
}


YUV_error char_to_overlay(p_info_rec* p_info, overlay* ovly, char character,
                          char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den)
{
    info_rec*       info;
    FT_GlyphSlot    slot;
    int         bb_t, bb_b, bb_l, bb_r; // bounding box
    BYTE*       dstLine;
    BYTE*       srcPtr;
    BYTE*       dstPtr;
    int         j;
    int         result;

    // initialise our data
    if (*p_info == NULL)
    {
        result = allocate_info(p_info);
        if (result < 0)
            return result;
    }
    info = *p_info;
    // create a suitable text image
    result = set_font(info, font, size, aspect_ratio_num, aspect_ratio_den);
    if (result < 0)
        return result;
    // get bounding box for character
    bb_t =  1000000;
    bb_b = -1000000;
    bb_l =  1000000;
    bb_r = -1000000;
    /* load glyph image into the slot (erase previous one) */
    if (FT_Load_Char(info->face, character, FT_LOAD_RENDER))
        return YUV_freetype;
    slot = info->face->glyph;  /* a small shortcut */
    if (bb_t > -slot->bitmap_top)
        bb_t = -slot->bitmap_top;
    if (bb_b < slot->bitmap.rows - slot->bitmap_top)
        bb_b = slot->bitmap.rows - slot->bitmap_top;
    if (bb_l > slot->bitmap_left)
        bb_l = slot->bitmap_left;
    if (bb_r < slot->bitmap_left + slot->bitmap.width)
        bb_r = slot->bitmap_left + slot->bitmap.width;
    // expand bounding box a little
    bb_t -= 1;
    bb_b += 1;
    bb_l -= 1;
    bb_r += 1;
    // initialise character overlays
    ovly->w = bb_r - bb_l;
    ovly->h = bb_b - bb_t;
    ovly->ssx = -1;
    ovly->ssy = -1;
    ovly->buff = malloc(ovly->w * ovly->h * 2);
    if (ovly->buff == NULL)
        return YUV_no_memory;
    memset(ovly->buff, 0, ovly->w * ovly->h * 2);
    ovly->Cbuff = NULL;
    // copy bitmap
    /* load glyph image into the slot (erase previous one) */
    if (FT_Load_Char(info->face, character, FT_LOAD_RENDER))
        return YUV_freetype;
    slot = info->face->glyph;  /* a small shortcut */
    if (character == '(' || character == ')')  // add other characters to this list
    {
        // make colon narrower than other characters
        ovly->w = slot->advance.x / 64;
    }
    srcPtr = slot->bitmap.buffer;
    dstLine = ovly->buff;
    // add vertical offset
    dstLine += ovly->w * (-slot->bitmap_top - bb_t);
    // horizontally centre character
    dstLine += (ovly->w - slot->bitmap.width) / 2;
    for (j = 0; j < slot->bitmap.rows; j++)
    {
        dstPtr = dstLine;
        memcpy(dstLine, srcPtr, slot->bitmap.width);
        srcPtr += slot->bitmap.width;
        dstLine += ovly->w;
    }
    return YUV_OK;
}

void free_char_set(char_set_data* cs_data)
{
    int c;

    if (cs_data == NULL)
        return;

    for (c = 0; c < cs_data->numChars; c++)
        free_overlay(&cs_data->cs_ovly[c]);
}

YUV_error char_set_to_overlay(p_info_rec* p_info, char_set_data* cs_data,
                          char* cset, char* font, const int size,
                          const int aspect_ratio_num,
                          const int aspect_ratio_den)
{
    info_rec*       info;
    FT_GlyphSlot    slot;
    int         bb_t, bb_b, bb_l, bb_r; // bounding box
    BYTE*       dstLine;
    BYTE*       srcPtr;
    BYTE*       dstPtr;
    int         c, j;
    int         result;
    int         csetLen;

    // check maximum chars not exceeded
    csetLen = (int)strlen(cset);
    if (csetLen > MAX_CHARS_IN_SET)
    {
        return YUV_size_error;
    }
    // initialise our data
    if (*p_info == NULL)
    {
        result = allocate_info(p_info);
        if (result < 0)
            return result;
    }
    info = *p_info;
    // create a suitable text image
    result = set_font(info, font, size, aspect_ratio_num, aspect_ratio_den);
    if (result < 0)
        return result;
    // get bounding box for characters
    bb_t =  1000000;
    bb_b = -1000000;
    bb_l =  1000000;
    bb_r = -1000000;
    for (c = 0; c < csetLen; c++)
    {
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(info->face, cset[c], FT_LOAD_RENDER))
            return YUV_freetype;
        slot = info->face->glyph;  /* a small shortcut */
        if (bb_t > -slot->bitmap_top)
            bb_t = -slot->bitmap_top;
        if (bb_b < slot->bitmap.rows - slot->bitmap_top)
            bb_b = slot->bitmap.rows - slot->bitmap_top;
        if (bb_l > slot->bitmap_left)
            bb_l = slot->bitmap_left;
        if (bb_r < slot->bitmap_left + slot->bitmap.width)
            bb_r = slot->bitmap_left + slot->bitmap.width;
    }
    // expand bounding box a little
    bb_t -= 1;
    bb_b += 1;
    bb_l -= 1;
    bb_r += 1;
    // initialise character overlays
    for (c = 0; c < csetLen; c++)
    {
        cs_data->cs_ovly[c].w = bb_r - bb_l;
        cs_data->cs_ovly[c].h = bb_b - bb_t;
        cs_data->cs_ovly[c].ssx = -1;
        cs_data->cs_ovly[c].ssy = -1;
        cs_data->cs_ovly[c].buff = malloc(cs_data->cs_ovly[c].w *
                                          cs_data->cs_ovly[c].h * 2);
        if (cs_data->cs_ovly[c].buff == NULL)
            return YUV_no_memory;
        memset(cs_data->cs_ovly[c].buff, 0, cs_data->cs_ovly[c].w *
                                            cs_data->cs_ovly[c].h * 2);
        cs_data->cs_ovly[c].Cbuff = NULL;
    }
    // copy bitmaps
    for (c = 0; c < csetLen; c++)
    {
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(info->face, cset[c], FT_LOAD_RENDER))
            return YUV_freetype;
        slot = info->face->glyph;  /* a small shortcut */
        if (c == 10)
        {
            // make colon narrower than other characters
            cs_data->cs_ovly[c].w = slot->advance.x / 64;
        }
        srcPtr = slot->bitmap.buffer;
        dstLine = cs_data->cs_ovly[c].buff;
        // add vertical offset
        dstLine += cs_data->cs_ovly[c].w * (-slot->bitmap_top - bb_t);
        // horizontally centre character
        dstLine += (cs_data->cs_ovly[c].w - slot->bitmap.width) / 2;
        for (j = 0; j < slot->bitmap.rows; j++)
        {
            dstPtr = dstLine;
            memcpy(dstLine, srcPtr, slot->bitmap.width);
            srcPtr += slot->bitmap.width;
            dstLine += cs_data->cs_ovly[c].w;
        }
    }

    memcpy(cs_data->chars, cset, csetLen);
    cs_data->numChars = csetLen;

    return YUV_OK;
}

