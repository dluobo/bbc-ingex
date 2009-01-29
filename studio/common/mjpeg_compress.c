/*
 * $Id: mjpeg_compress.c,v 1.3 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * MJPEG encoder.
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

// Compile and link to libjpeg library:
//  gcc -Wall -g -O3 uyvy_to_mjpeg.c -o uyvy_to_mjpeg -ljpeg
//
// Or use the mmx library
//  gcc -Wall -g -O3 uyvy_to_mjpeg.c -o uyvy_to_mjpeg-mmx -L/home/stuartc/pub/jpeg-mmx-cvs -ljpeg-mmx

// fseeko() requires _LARGEFILE_SOURCE defined before including stdio.h
#if ! defined(_LARGEFILE_SOURCE) && ! defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "mjpeg_compress.h"
#include "YUVlib/YUV_scale_pic.h"

// Utility functions for dealing with Avid's JPEG header format
static void storeUInt16_BE(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)((value & 0x0000ff00) >> 8);
    p[1] = (uint8_t)((value & 0x000000ff) >> 0);
}

static void storeUInt32_BE(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)((value & 0xff000000) >> 24);
    p[1] = (uint8_t)((value & 0x00ff0000) >> 16);
    p[2] = (uint8_t)((value & 0x0000ff00) >> 8);
    p[3] = (uint8_t)((value & 0x000000ff) >> 0);
}

// Customised JPEG destination for writing a pair of fields to a single memory buffer
typedef struct {
    struct jpeg_destination_mgr pub;    /* public fields */
    JOCTET * buffer;                    /* temporary buffer for single image before it is fixed */
    uint8_t *pair_buffer;           // pair of output compressed images concatenated together
    int field_number;                   // toggles between 0 and 1 recording current field
    int resolution_id;                  // needed when writing fixed Avid jpeg
    int last_jpeg_size;                 // private data for Avid MJPEG specifics
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE 1*1024*1024     // bigger than largest expected jpeg

static void custom_init_destination(j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

    /* Allocate temporary buffer - it will be released when done with image */
    dest->buffer = (JOCTET *)
        (*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                    OUTPUT_BUF_SIZE * sizeof(JOCTET));

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/* this will never be called unless compressed image is very large */
static boolean custom_empty_output_buffer (j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

    fprintf(stderr, "custom_empty_output_buffer: Overflowed OUTPUT_BUF_SIZE (%d) - JPEG corrupted\n", OUTPUT_BUF_SIZE);
    
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    
    return TRUE;
}


// Called by jpeg_finish_compress()
// So will be called when compressing many images one after another
// such a sequence of compressions is terminated by jpeg_destroy_compress()
//
static void custom_term_destination (j_compress_ptr cinfo)
{
    my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
    int new_size;

    // First commpressed image is written to start of output buffer
    // Second image is written immediately after the first
    unsigned char *p_fixed;
    if (dest->field_number == 0) {          // dealing with first field
        p_fixed = dest->pair_buffer;
        dest->field_number = 1;             // indicate next field will follow
    }
    else {                                  // dealing with second field
        p_fixed = dest->pair_buffer + dest->last_jpeg_size;
        dest->field_number = 0;
    }

    /* fix jpeg header for Avid MJPEG constraints */
    new_size = mjpeg_fix_jpeg(dest->buffer, datacount, dest->last_jpeg_size, dest->resolution_id, p_fixed);

    dest->last_jpeg_size = new_size;
}

/* setup cinfo and destination manager */
/* Pass in a pointer for the output compressed image */
static void jpeg_custom_dest(j_compress_ptr cinfo, int resolution_id, uint8_t *output)
{
    my_dest_ptr dest;
    
    /* The destination object is made permanent so that multiple JPEG images
     * can be written to the same file without re-executing jpeg_stdio_dest.
     * This makes it dangerous to use this manager and a different destination
     * manager serially with the same JPEG object, because their private object
     * sizes may be different.  Caveat programmer.
     */
    if (cinfo->dest == NULL) {  /* first time for this JPEG object? */
      cinfo->dest = (struct jpeg_destination_mgr *)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                  sizeof(my_destination_mgr));
    }
    
    dest = (my_dest_ptr) cinfo->dest;
    dest->pub.init_destination = custom_init_destination;
    dest->pub.empty_output_buffer = custom_empty_output_buffer;
    dest->pub.term_destination = custom_term_destination;
    dest->pair_buffer = output;
    dest->field_number = 0;
    dest->resolution_id = resolution_id;
    dest->last_jpeg_size = 0;           // for Avid MJPEG compressed size
}

#define ROUND_UP_TO_DCTSIZE(x) ( ((x + DCTSIZE-1) / DCTSIZE) * DCTSIZE )
extern int mjpeg_compress_init(MJPEGResolutionID id, int width, int height, mjpeg_compress_t **pp_handle)
{
    int i;

    // allocate memory for mjpeg handle
    *pp_handle = (mjpeg_compress_t *)malloc(sizeof(mjpeg_compress_t));

    // temp pointer for clearer syntax
    mjpeg_compress_t *p = *pp_handle;

    // setup object variables
    p->resID = id;
    p->src_height = height;
    p->single_field = 0;
    if (p->resID == MJPEG_15_1s || p->resID == MJPEG_10_1m || p->resID == MJPEG_4_1m)
        p->single_field = 1;

    p->cinfo.err = jpeg_std_error(&p->jerr);
    jpeg_create_compress(&p->cinfo);

    // Setup customized destination manager which simply writes to
    // a large memory buffer
    uint8_t *picture_pair = malloc(OUTPUT_BUF_SIZE);
    jpeg_custom_dest(&p->cinfo, p->resID, picture_pair);

    int jpeg_width = width;

    // Single field formats must be subsampled to near half-width
    // PAL: 288x296 (4:1m) 352x296 (15:1s)
    // NTSC: 288x248
    if (p->single_field) {
        // Image must be subsampled horizontally
        if (p->resID == MJPEG_15_1s)
            jpeg_width = ROUND_UP_TO_DCTSIZE(352);
        else
            jpeg_width = ROUND_UP_TO_DCTSIZE(288);
        int c_width = ROUND_UP_TO_DCTSIZE(jpeg_width / 2);

        // Allocate space for temp half-width, one field, video buffer,
        // rounding up to nearest DCTSIZE (i.e. multiple of 8)
        int y_size = jpeg_width * p->src_height / 2;
        int c_size = c_width * p->src_height / 2;
        p->half_y = malloc(y_size);
        p->half_u = malloc(c_size);
        p->half_v = malloc(c_size);

        // Clear unused space at end of lines to black
        memset(p->half_y, 0x10, y_size);
        memset(p->half_u, 0x80, c_size);
        memset(p->half_v, 0x80, c_size);

        // Also allocate a temp workspace for YUV scale_pic
        p->workspace = malloc(width * 4);
    }

    p->cinfo.image_width = jpeg_width;
    // Add 16 lines VBI for Avid, half height for one field
    p->cinfo.image_height = (p->src_height + 16) / 2;

    // Setup black buffer for one field (VBI has 16 lines, one field has 8)
    for (i = 0; i < 8; i++) {
        int line_width = ROUND_UP_TO_DCTSIZE(jpeg_width);
        int color_width = ROUND_UP_TO_DCTSIZE(jpeg_width / 2);
        p->black_y[i] = malloc(line_width);
        p->black_u[i] = malloc(color_width);
        p->black_v[i] = malloc(color_width);

        int x;
        for (x = 0; x < line_width; x++) {
            p->black_y[i][x] = 0x10;
            if (x < color_width) {      // chroma is subsampled
                p->black_u[i][x] = 0x80;
                p->black_v[i][x] = 0x80;
            }
        }
    }
    p->black_buf[0] = p->black_y;
    p->black_buf[1] = p->black_u;
    p->black_buf[2] = p->black_v;

    return 1;
}

extern int mjpeg_compress_free(mjpeg_compress_t *p)
{
    int i;
    my_dest_ptr dest;

    dest = (my_dest_ptr) p->cinfo.dest;
    free(dest->pair_buffer);

    jpeg_destroy_compress(&(p->cinfo));

    for (i = 0; i < 8; i++) {
        free(p->black_y[i]);
        free(p->black_u[i]);
        free(p->black_v[i]);
    }

    if (p->single_field) {
        free(p->half_y);
        free(p->half_u);
        free(p->half_v);
        free(p->workspace);
    }

    free(p);

    return 1;
}

/* Given a frame of YUV422, encode the specified field (0 or 1) */
/* returns size of compressed jpeg image, or 0 on failure */
static unsigned mjpeg_compress_field_yuv(
                                mjpeg_compress_t *p,
                                const unsigned char *y,
                                const unsigned char *u,
                                const unsigned char *v,
                                int y_linesize,
                                int u_linesize,
                                int v_linesize,
                                int field,
                                int quality)
{
    int i;

    j_compress_ptr cinfo = &(p->cinfo);

    cinfo->input_components = 3;
    cinfo->raw_data_in = TRUE;
    
    jpeg_set_defaults(cinfo);
    jpeg_set_quality(cinfo, quality, TRUE /* limit to baseline-JPEG values */);
    jpeg_set_colorspace(cinfo, JCS_YCbCr);

    // component 0 (Y) set to 2x1 (4:2:2) sampling
    cinfo->raw_data_in = TRUE;          // supply downsampled data
    cinfo->comp_info[0].h_samp_factor = 2;
    cinfo->comp_info[0].v_samp_factor = 1;
    cinfo->comp_info[1].h_samp_factor = 1;
    cinfo->comp_info[1].v_samp_factor = 1;
    cinfo->comp_info[2].h_samp_factor = 1;
    cinfo->comp_info[2].v_samp_factor = 1;

    jpeg_start_compress(cinfo, TRUE);

    // cinfo->max_v_samp_factor == 1 for 4:2:2 since all buffers
    // allocated assuming 4:2:2
    assert(cinfo->max_v_samp_factor == 1);

    uint8_t *ya[DCTSIZE];
    uint8_t *ua[DCTSIZE];
    uint8_t *va[DCTSIZE];
    JSAMPARRAY      buf[3];

    buf[0] = ya;
    buf[1] = ua;
    buf[2] = va;


    // Compress 8 lines of black at top of picture, needed for Avid compatibility
    jpeg_write_raw_data(cinfo, p->black_buf, cinfo->max_v_samp_factor * DCTSIZE);

    // For single-field formats, like 15:1s, subsample original image horizontally
    // and use temp buffers for compression source
    if (p->single_field) {
        // More general subsampling e.g. for 10:1m
        YUV_frame input_frame;
        input_frame.Y.w = y_linesize;
        input_frame.Y.h = p->src_height;
        input_frame.Y.lineStride = y_linesize;
        input_frame.Y.pixelStride = 1;
        input_frame.Y.buff = (unsigned char *)y;
        input_frame.U.w = u_linesize;
        input_frame.U.h = p->src_height;
        input_frame.U.lineStride = u_linesize;
        input_frame.U.pixelStride = 1;
        input_frame.U.buff = (unsigned char *)u;
        input_frame.V.w = v_linesize;
        input_frame.V.h = p->src_height;
        input_frame.V.lineStride = v_linesize;
        input_frame.V.pixelStride = 1;
        input_frame.V.buff = (unsigned char *)v;

        YUV_frame input_field; // single field
        extract_YUV_field(&input_frame, &input_field, 0);

        YUV_frame output_frame;
        output_frame.Y.w = cinfo->image_width;
        output_frame.Y.h = p->src_height / 2;
        output_frame.Y.lineStride = cinfo->image_width;
        output_frame.Y.pixelStride = 1;
        output_frame.Y.buff = p->half_y;
        output_frame.U.w = cinfo->image_width / 2;
        output_frame.U.h = p->src_height / 2;
        output_frame.U.lineStride = cinfo->image_width / 2;
        output_frame.U.pixelStride = 1;
        output_frame.U.buff = p->half_u;
        output_frame.V.w = cinfo->image_width / 2;
        output_frame.V.h = p->src_height / 2;
        output_frame.V.lineStride = cinfo->image_width / 2;
        output_frame.V.pixelStride = 1;
        output_frame.V.buff = p->half_v;

        scale_pic(&input_field, &output_frame,
            0, 0, output_frame.Y.w, output_frame.Y.h,
            0, 1, 1,				// intlc, hfil, vfil
            p->workspace);

        while (cinfo->next_scanline < cinfo->image_height) {
            for (i = 0; i < DCTSIZE; i++) {
                int line = field + (i + cinfo->next_scanline - 8);
                ya[i] = (uint8_t *)p->half_y + line * cinfo->image_width;
                ua[i] = (uint8_t *)p->half_u + line * cinfo->image_width/2;
                va[i] = (uint8_t *)p->half_v + line * cinfo->image_width/2;
            }
            jpeg_write_raw_data(cinfo, buf, cinfo->max_v_samp_factor * DCTSIZE);
        }
    }
    else {
        // Compressed chunk of DCTSIZE (8) lines of uncompressed data per iteration
        // Source is YUV422 mixed fields
        while (cinfo->next_scanline < cinfo->image_height) {
            for (i = 0; i < DCTSIZE; i++) {
                // line is the index into the original image.
                // We subtract 8 for the 8 blank lines already written and
                // multiply by 2 to skip alternate lines i.e. use one field only.
                int line = field + (i + cinfo->next_scanline - 8) * 2;
                ya[i] = (uint8_t *)y + line * y_linesize;
                ua[i] = (uint8_t *)u + line * u_linesize;
                va[i] = (uint8_t *)v + line * v_linesize;
            }
            jpeg_write_raw_data(cinfo, buf, cinfo->max_v_samp_factor * DCTSIZE);
        }
    }
    jpeg_finish_compress(cinfo);

    if (p->single_field) {
        // handle single field formats by only compressing the top field
        // and resetting the dest->field_number to 0 after each compression
        ((my_dest_ptr)cinfo->dest)->field_number = 0;
    }

    int compressed_size = ((my_dest_ptr)cinfo->dest)->last_jpeg_size;
    return compressed_size;
}

static int resolution_id_to_quality(int resID)
{
    // Resolution Id to bitrate based on experiment and the following table
    // Avid Help has a "Estimated Storage Requirements: JFIF Interlaced" page
    // which gives storage size per 30 minutes of video (1800 sec, 45000 frames)
    // 4th column is Avid generated from DV50 capture at Elstree.
    //
    // 20:1     1.80 GB     8.6 Mbps    7.18 Mbps
    // 10:1     3.60 GB     17.2 Mbps
    // 3:1      10.32 GB    49.2 Mbps
    // 2:1      15.48 GB    73.9 Mbps
    // 1:1      36.6  GB    174.7 Mbps
    // 1:1 10b  45.9  GB    219.0 Mbps
    //
    // 15:1s    0.618GB     2.9 Mbps
    // 10:1m    ?           ?           2.86 MBps (288x296) (identify gives qual=85)
    // 4:1m     ?           ?           35800 bytes per frame average

    int quality;
    switch (resID) {
        case MJPEG_2_1: quality = 98; break;        // 2:1
        case MJPEG_3_1: quality = 97; break;        // 3:1
        case MJPEG_10_1: quality = 84; break;       // 10:1
        case MJPEG_20_1: quality = 68; break;       // 20:1     gives 13.3Mbps for elstree sample
        case MJPEG_15_1s: quality = 55; break;      // 15:1s
        case MJPEG_10_1m: quality = 71; break;      // 10:1m
        case MJPEG_4_1m: quality = 94; break;       // 4:1m
        default:
                fprintf(stderr, "Unknown resolution ID %d, compression failed\n", resID);
                return 0;
                break;
    }

	return quality;
}

extern unsigned mjpeg_compress_frame_yuv(
                                mjpeg_compress_t *p,
                                const unsigned char *y,
                                const unsigned char *u,
                                const unsigned char *v,
                                int y_linesize,
                                int u_linesize,
                                int v_linesize,
                                unsigned char **pp_output)
{
	int quality = resolution_id_to_quality(p->resID);

    unsigned total_size = 0;
    j_compress_ptr cinfo = &(p->cinfo);

    // compress top field
	unsigned top_size = 0, bot_size = 0;
    top_size = mjpeg_compress_field_yuv(p, y, u, v, y_linesize, u_linesize, v_linesize, 0, quality);
	total_size += top_size;

    if (! p->single_field) {
        // compress bottom field
        bot_size += mjpeg_compress_field_yuv(p, y, u, v, y_linesize, u_linesize, v_linesize, 1, quality);
        total_size += bot_size;
    }

    *pp_output = ((my_dest_ptr)cinfo->dest)->pair_buffer;
    return total_size;
}

// Rearrange JPEG markers into the fashion needed by Avid:
//   APP0 (always 16 bytes incl. 2 byte segment length)
//   COM  (always 61 bytes)
//   DRI
//   DQT (Y table and UV table together in one segment)
//   DHT (always default libjpeg tables but all in one segment)
//   SOF0
//   SOS
//
extern int mjpeg_fix_jpeg(const JOCTET *p_in, int size, int last_size, int resolution_id, JOCTET *p_out)
{
    int     i, end_of_header = size;
    JOCTET  *p = p_out;

    // integrity check of JPEG picture
    if ( ! (p_in[0] == 0xFF && p_in[1] == 0xD8) ) {
        fprintf(stderr, "No SOI marker\n");
        return 0;
    }
    if ( ! (p_in[size-2] == 0xFF && p_in[size-1] == 0xD9) ) {
        fprintf(stderr, "No EOI marker\n");
        return 0;
    }

    // Find SOS marker and use it effectively as end-of-header
    for (i = 0; i < (size - 1); i++) {
        if (p_in[i] == 0xFF && p_in[i+1] == 0xDA) {
            end_of_header = i;
            break;
        }
    }

    // Start with SOI marker
    *p++ = 0xFF;
    *p++ = 0xD8;
    
    // Create Avid APP0 marker
    *p++ = 0xFF;
    *p++ = 0xE0;
    *p++ = 0x00;
    *p++ = 0x10;
    unsigned char app0_buf[14] = "AVI1";
    uint8_t *pos_app_data = p + 6;      // use this to update data later
    memcpy(p, app0_buf, sizeof(app0_buf));
    p += sizeof(app0_buf);

    // Create Avid COM marker
    *p++ = 0xFF;
    *p++ = 0xFE;
    *p++ = 0x00;
    *p++ = 0x3D;
    unsigned char com_buf[59] = "AVID\x11";
    com_buf[11] = resolution_id;                // ResolutionID 0x4C=2:1, 0x4E=15:1s etc
    com_buf[12] = 0x02;                         // Always 2
    storeUInt32_BE(&com_buf[7], last_size);     // store last_size
    memcpy(p, com_buf, sizeof(com_buf));
    p += sizeof(com_buf);

    // Insert DRI (appears necessary for Avid)
    *p++ = 0xFF;
    *p++ = 0xDD;
    *p++ = 0x00;
    *p++ = 0x04;
    *p++ = 0x00;
    *p++ = 0x00;

    // Copy DQT tables all as single segment
    *p++ = 0xFF;
    *p++ = 0xDB;
    uint8_t *pos_dqt_len = p;       // use this to update length later
    *p++ = 0x00;
    *p++ = 0x00;
    // Find DQT markers, copy contents to destination
    uint16_t dqt_len = 2;
    for (i = 0; i < end_of_header - 1; i++) {
        // search for DQT marker
        if (p_in[i] == 0xFF && p_in[i+1] == 0xDB) {
            // length is 16bit big-endian and includes storage of length (2 bytes)
            uint16_t length = (p_in[i+2] << 8) + p_in[i+3];
            length -= 2;            // compute length of table contents

            // copy contents of this table
            memcpy(p, &p_in[i+4], length);
            p += length;
            dqt_len += length;
        }
    }
    // update DQT segment length
    storeUInt16_BE(pos_dqt_len, dqt_len);

    // Copy DHT tables all as single segment
    // TODO: test whether Avid can handle different order of tables
    *p++ = 0xFF;
    *p++ = 0xC4;
    uint8_t *pos_dht_len = p;       // use this to update length later
    *p++ = 0x00;
    *p++ = 0x00;
    // Find DHT markers, copy contents to destination
    uint16_t dht_len = 2;
    uint8_t dht_table[2][2][512];
    int     dht_length[2][2];
    for (i = 0; i < end_of_header - 1; i++) {
        // search for DHT marker
        if (p_in[i] == 0xFF && p_in[i+1] == 0xC4) {
            // length is 16bit big-endian and includes storage of length (2 bytes)
            uint16_t length = (p_in[i+2] << 8) + p_in[i+3];
            length -= 2;            // compute length of table contents

            // Table class Tc and Table destination idenitifier Th follow length
            int Tc = p_in[i+4] >> 4;
            int Th = p_in[i+4] & 0x0f;

            // copy contents of this table
            assert(length <= 512);
            memcpy(&dht_table[Tc][Th], &p_in[i+4], length);
            dht_length[Tc][Th] = length;
        }
    }
    // Copy out ordered DHT segment in Avid order:
    //  Tc=0 Th=0
    //  Tc=0 Th=1
    //  Tc=1 Th=0
    //  Tc=1 Th=1
    int Tc, Th;
    for (Tc = 0; Tc < 2; Tc++)
        for (Th = 0; Th < 2; Th++) {
            memcpy(p, &dht_table[Tc][Th], dht_length[Tc][Th]);
            p += dht_length[Tc][Th];
            dht_len += dht_length[Tc][Th];
        }

    // update DHT segment length
    storeUInt16_BE(pos_dht_len, dht_len);

    // Copy first SOF0 segment found
    *p++ = 0xFF;
    *p++ = 0xC0;
    for (i = 0; i < end_of_header - 1; i++) {
        // search for SOF0 marker
        if (p_in[i] == 0xFF && p_in[i+1] == 0xC0) {
            // length is 16bit big-endian and includes storage of length (2 bytes)
            uint16_t length = (p_in[i+2] << 8) + p_in[i+3];
            memcpy(p, &p_in[i+2], length);
            p += length;
            break;
        }
    }

    // Copy SOS and entropy coded segment through to EOI marker
    memcpy(p, &p_in[end_of_header], size - end_of_header);
    p += size - end_of_header;

    // update Avid-special size metadata now that we know the new size
    int new_size = p - p_out;
    storeUInt32_BE(pos_app_data, new_size);
    storeUInt32_BE(pos_app_data+4, new_size);

    return new_size;
}

static void set_jpeg_last_size(uint8_t *jpegbuf, int last_size)
{
	// last size metadata always located at offset 32 (4 bytes long)
    storeUInt32_BE(jpegbuf + 31, last_size);     // store last_size
}

static void *mjpeg_compress_worker_thread(void *p_obj);

extern int mjpeg_compress_init_threaded(MJPEGResolutionID id, int width, int height, mjpeg_compress_threaded_t **pp_handle)
{
	// allocate memory for handle
    *pp_handle = (mjpeg_compress_threaded_t *)malloc(sizeof(mjpeg_compress_threaded_t));
	mjpeg_thread_data_t *thread = (*pp_handle)->thread;

	/* Setup thread-specific data for each thread */
	int i;
	for (i = 0; i < 2; i++) {
		if (! mjpeg_compress_init(id, width, height, &thread[i].handle))
			return 0;

		thread[i].thread_num = i;

		pthread_mutex_init(&thread[i].m_state_change, NULL);
		pthread_cond_init(&thread[i].state_change, NULL);
		thread[i].state = THREAD_INIT;

		thread[i].compressed_size = 0;
		thread[i].last_compressed_size = 0;

		/* Create two worker threads which will start out waiting for an input frame */
		int res;
		if ((res = pthread_create(	&thread[i].thread_id, NULL,
									mjpeg_compress_worker_thread, &thread[i])) != 0) {
        	fprintf(stderr, "Failed to create worker thread: %s\n", strerror(res));
        	return 0;
		}
	}

	return 1;
}

static void *mjpeg_compress_worker_thread(void *p_obj)
{
    mjpeg_thread_data_t *p = (mjpeg_thread_data_t*)(p_obj);
	int count = 0;

	while (1) {
		/* wait for next frame to encode */
		while (p->state != THREAD_INPUT_READY) {

			pthread_mutex_lock(&p->m_state_change);
			if (p->state == THREAD_FINI) {
				pthread_mutex_unlock(&p->m_state_change);
				pthread_exit(NULL);
			}
			if (p->state != THREAD_INPUT_READY)
				pthread_cond_wait(&p->state_change, &p->m_state_change);
			pthread_mutex_unlock(&p->m_state_change);
		}

		/* encode frame (thread_num determines top / bottom field) */
		/* resulting compressed size is stored in thread data */
		p->state = THREAD_BUSY;
		pthread_mutex_lock(&p->m_state_change);
		pthread_cond_signal(&p->state_change);
		pthread_mutex_unlock(&p->m_state_change);

		p->last_compressed_size = p->compressed_size;
		p->compressed_size = mjpeg_compress_field_yuv(p->handle, p->y, p->u, p->v, p->y_linesize, p->u_linesize, p->v_linesize, p->thread_num, p->quality);

        // reset field_number to 0 to treat as single-field compression
    	j_compress_ptr cinfo = &(p->handle->cinfo);
        ((my_dest_ptr)cinfo->dest)->field_number = 0;

		/* signal compressed frame is ready */
		p->state = THREAD_OUTPUT_READY;
		count++;
		pthread_mutex_lock(&p->m_state_change);
		pthread_cond_signal(&p->state_change);
		pthread_mutex_unlock(&p->m_state_change);
	}

    return NULL;
}

extern unsigned mjpeg_compress_frame_yuv_threaded(
                                mjpeg_compress_threaded_t *p,
                                const unsigned char *y,
                                const unsigned char *u,
                                const unsigned char *v,
                                int y_linesize,
                                int u_linesize,
                                int v_linesize,
                                unsigned char **pp_output)
{
	mjpeg_thread_data_t *thread = p->thread;
	int quality = resolution_id_to_quality(thread[0].handle->resID);

	// Setup inputs
	int i;
	for (i = 0; i < 2; i++) {
		thread[i].y = y;
		thread[i].u = u;
		thread[i].v = v;
		thread[i].y_linesize = y_linesize;
		thread[i].u_linesize = u_linesize;
		thread[i].v_linesize = v_linesize;
		thread[i].quality = quality;
	}

	for (i = 0; i < 2; i++) {
    	// Signal worker to compress a field
		thread[i].state = THREAD_INPUT_READY;
		pthread_mutex_lock(&thread[i].m_state_change);
		pthread_cond_signal(&thread[i].state_change);
		pthread_mutex_unlock(&thread[i].m_state_change);
	}
	
	// Wait for both threads to finish, getting the compressed sizes
	for (i = 0; i < 2; i++) {
		while (thread[i].state != THREAD_OUTPUT_READY) {
			pthread_mutex_lock(&thread[i].m_state_change);
			if (thread[i].state != THREAD_OUTPUT_READY)
				pthread_cond_wait(&thread[i].state_change, &thread[i].m_state_change);
			pthread_mutex_unlock(&thread[i].m_state_change);
		}
	}
	int top_size = thread[0].compressed_size;
	int bottom_size = thread[1].compressed_size;

	// Serialise results by copying bottom field after top field
	j_compress_ptr cinfo = &(thread[0].handle->cinfo);
	uint8_t *top_buffer = ((my_dest_ptr)cinfo->dest)->pair_buffer;
	cinfo = &(thread[1].handle->cinfo);
	uint8_t *bottom_buffer = ((my_dest_ptr)cinfo->dest)->pair_buffer;
	memcpy(top_buffer + top_size, bottom_buffer, bottom_size);

	// update last_size metadata in JPEG headers
	set_jpeg_last_size(top_buffer, thread[1].last_compressed_size);
	set_jpeg_last_size(top_buffer + top_size, top_size);

    *pp_output = top_buffer;
    return top_size + bottom_size;
}

extern int mjpeg_compress_free_threaded(mjpeg_compress_threaded_t *p)
{
	mjpeg_thread_data_t *thread = p->thread;

    int i;
	for (i = 0; i < 2; i++) {
		mjpeg_compress_free(thread[i].handle);
		thread[i].state = THREAD_FINI;
		pthread_mutex_lock(&thread[i].m_state_change);
		pthread_cond_signal(&thread[i].state_change);
		pthread_mutex_unlock(&thread[i].m_state_change);
		pthread_join(thread[i].thread_id, NULL);
	}

	free(p);
    return 1;
}
