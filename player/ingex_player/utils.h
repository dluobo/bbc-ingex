/*
 * $Id: utils.h,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Author: Stuart Cunningham
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif


#include "frame_info.h"


#define MAX_TIMESTAMP_STRING_SIZE   20


/* returns number of usecs slept. value can be negative */
long sleep_diff(long delay, const struct timeval* now, const struct timeval* last);

void print_timecode(TimecodeType type, TimecodeSubType subType, const Rational* frameRate, const Timecode* timecode);

void get_timestamp_string(char* timestampStr);
void get_short_timestamp_string(char* timestampStr);
int64_t get_timecode_now(int roundedFrameRate);


int init_mutex(pthread_mutex_t* mutex);
int init_cond_var(pthread_cond_t* cond);

void destroy_mutex(pthread_mutex_t* mutex);
void destroy_cond_var(pthread_cond_t* cond);

int create_joinable_thread(pthread_t* thread, void* (*start_func)(void*), void* arg);
void cancel_and_join_thread(pthread_t* thread, void* data, void (*stop_func)(void*));
void join_thread(pthread_t* thread, void* data, void (*stop_func)(void*));


void writeVITC(unsigned hh, unsigned mm, unsigned ss, unsigned ff, unsigned char *line);

double calc_audio_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power);
double calc_audio_peak_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power);

void strcat_separator(char* path);

char* get_host_name(char* buffer, size_t bufferSize);
char* get_user_name(char* buffer, size_t bufferSize);


int64_t convert_length(int64_t length, const Rational* oldFrameRate, const Rational* newFrameRate);
int64_t convert_non_drop_timecode(int64_t timecode, const Rational* oldFrameRate, const Rational* newFrameRate);



#ifdef __cplusplus
}
#endif


#endif



