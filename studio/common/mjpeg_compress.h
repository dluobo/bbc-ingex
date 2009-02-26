/*
 * $Id: mjpeg_compress.h,v 1.2 2009/02/26 19:28:07 john_f Exp $
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

#ifndef MJPEG_COMPRESS_H
#define MJPEG_COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <jpeglib.h>
#include <pthread.h>

typedef enum MJPEGResolutionID
{
    MJPEG_2_1   = 76,
    MJPEG_3_1   = 77,
    MJPEG_10_1  = 75,
    MJPEG_20_1  = 82,
    MJPEG_15_1s = 78,
    MJPEG_10_1m = 110,
    MJPEG_4_1m  = 111
} MJPEGResolutionID;

typedef struct mjpeg_compress_t {
    MJPEGResolutionID resID;
    int single_field;       // flag to indicate single-field format e.g. 15:1s
    int src_height;         // height of source image to be passed
    unsigned char *half_y;
    unsigned char *half_u;
    unsigned char *half_v;
    unsigned char *workspace;
    JSAMPROW black_y[DCTSIZE];
    JSAMPROW black_u[DCTSIZE];
    JSAMPROW black_v[DCTSIZE];
    JSAMPARRAY black_buf[3];
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;
} mjpeg_compress_t;

extern int mjpeg_compress_init(MJPEGResolutionID id, int width, int height, mjpeg_compress_t **);

extern int mjpeg_compress_free(mjpeg_compress_t *);

/* Given a frame of YUV422, encode it to mjpeg (i.e. two pictures each of one field) */
/* Returns the size of the compressed pair of images or 0 on failure */
extern unsigned mjpeg_compress_frame_yuv(
                                mjpeg_compress_t *p,
                                const unsigned char *y,
                                const unsigned char *u,
                                const unsigned char *v,
                                int y_linesize,
                                int u_linesize,
                                int v_linesize,
                                unsigned char **pp_output);


/* Multi-threaded API consists of 3 functions
 * - mjpeg_compress_init_threaded
 * - mjpeg_compress_frame_yuv_threaded
 * - mjpeg_compress_free_threaded
 */

typedef enum {
	THREAD_INIT,
	THREAD_INPUT_READY,
	THREAD_BUSY,
	THREAD_OUTPUT_READY,
	THREAD_FINI
} mjpeg_thread_state_t;

typedef struct mjpeg_thread_data_t {
	/* thread management */
    mjpeg_compress_t *handle;
    int thread_num;						/* 0 or 1 */
	pthread_t thread_id;				/* for creating and joining threads */
    pthread_mutex_t m_state_change;
    pthread_cond_t state_change;
	mjpeg_thread_state_t state;

	/* field compression inputs */
	const unsigned char *y;
    const unsigned char *u;
    const unsigned char *v;
    int y_linesize;
    int u_linesize;
    int v_linesize;

	/* field compression outputs */
	int compressed_size;
	int last_compressed_size;
} mjpeg_thread_data_t;

typedef struct mjpeg_compress_threaded_t {
	mjpeg_thread_data_t thread[2];
} mjpeg_compress_threaded_t;

extern int mjpeg_compress_init_threaded(MJPEGResolutionID id, int width, int height, mjpeg_compress_threaded_t **);

extern int mjpeg_compress_free_threaded(mjpeg_compress_threaded_t *);

/* Threaded version of mjpeg_compress_frame_yuv() which encodes each
 * field in a different thread (2 threads in total)
 * No advantage is gained for single-field JPEG resolutions such as 15:1s
 */
extern unsigned mjpeg_compress_frame_yuv_threaded(
                                mjpeg_compress_threaded_t *p,
                                const unsigned char *y,
                                const unsigned char *u,
                                const unsigned char *v,
                                int y_linesize,
                                int u_linesize,
                                int v_linesize,
                                unsigned char **pp_output);

/* Convert a 'normal' YUV422 JPEG image into an Avid compatible JPEG */
extern int mjpeg_fix_jpeg(  const unsigned char *in,
                            int size,
                            int last_size,
                            int resolution_id,
                            unsigned char *out);

#ifdef __cplusplus
}
#endif

#endif //#ifndef MJPEG_COMPRESS_H
