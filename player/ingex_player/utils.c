/*
 * $Id: utils.c,v 1.5 2009/10/12 16:06:30 philipn Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>     /* usleep() */
#include <time.h>
#include <math.h>
#include <ctype.h>

#include "utils.h"
#include "logging.h"
#include "macros.h"


long sleep_diff(long delay, const struct timeval* now, const struct timeval* last)
{
    long diff_usec;

    diff_usec = (now->tv_sec - last->tv_sec) * 1000000 + now->tv_usec - last->tv_usec;

    if (diff_usec > 0 && diff_usec < delay)
    {
        usleep(delay - diff_usec);
        return delay - diff_usec;
    }
    else
    {
        //fprintf(stderr, "Missed frame point\n");
        if (diff_usec < 0)
        {
            return 0;
        }
        return delay - diff_usec;
    }
}



void print_timecode(TimecodeType type, TimecodeSubType subType, const Rational* frameRate, const Timecode* timecode)
{
    if (type == CONTROL_TIMECODE_TYPE)
    {
        printf("[C");
    }
    else if (type == SOURCE_TIMECODE_TYPE)
    {
        printf("[S");
    }
    else if (type == SYSTEM_TIMECODE_TYPE)
    {
        printf("[X");
    }
    else
    {
        printf("[?");
    }
    if (subType == VITC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-VITC");
    }
    else if (subType == LTC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-LTC");
    }
    else if (subType == DVITC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-DVITC");
    }
    else if (subType == DLTC_SOURCE_TIMECODE_SUBTYPE)
    {
        printf("-DLTC");
    }
    printf("] ");
    if (is_pal_frame_rate(frameRate))
    {
        printf("%02u:%02u:%02u:%02u", timecode->hour, timecode->min, timecode->sec, timecode->frame);
    }
    else
    {
        printf("%02u;%02u;%02u;%02u", timecode->hour, timecode->min, timecode->sec, timecode->frame);
    }
}

void get_timestamp_string(char* timestampStr)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        perror("time");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        fprintf(stderr, "localtime() failed\n");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }

    sprintf(timestampStr, "%d-%02d-%02d %02d:%02d:%02d", ltm->tm_year + 1900,
        ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);

}

void get_short_timestamp_string(char* timestampStr)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        perror("time");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        fprintf(stderr, "localtime() failed\n");
        timestampStr[0] = '\0'; /* func has no return value so we modify the string */
        return;
    }

    sprintf(timestampStr, "%d%02d%02d_%02d%02d%02d", ltm->tm_year + 1900,
        ltm->tm_mon + 1, ltm->tm_mday, ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
}

int64_t get_timecode_now(int roundedFrameRate)
{
    time_t t;
    struct tm* ltm;

    if (time(&t) == ((time_t)-1))
    {
        return 0;
    }
    if ((ltm = localtime(&t)) == NULL)
    {
        return 0;
    }

    return ltm->tm_hour * 60 * 60 * roundedFrameRate + ltm->tm_min * 60 * roundedFrameRate + ltm->tm_sec * roundedFrameRate;
}

int init_mutex(pthread_mutex_t* mutex)
{
    int err;

    if ((err = pthread_mutex_init(mutex, NULL)) != 0)
    {
        ml_log_error("Failed to initialise mutex: %s\n", strerror(err));
        return 0;
    }
    return 1;
}

int init_cond_var(pthread_cond_t* cond)
{
    int err;

    if ((err = pthread_cond_init(cond, NULL)) != 0)
    {
        ml_log_error("Failed to initialise conditional variable: %s\n", strerror(err));
        return 0;
    }
    return 1;
}

void destroy_mutex(pthread_mutex_t* mutex)
{
    int err;

    if (mutex == NULL)
    {
        return;
    }

	if ((err = pthread_mutex_destroy(mutex)) != 0)
    {
        ml_log_warn("Failed to destroy mutex: %s\n", strerror(err));
    }
}

void destroy_cond_var(pthread_cond_t* cond)
{
    int err;

    if (cond == NULL)
    {
        return;
    }

    if ((err = pthread_cond_destroy(cond) != 0))
    {
        ml_log_warn("Failed to destroy conditional variable: %s\n", strerror(err));
    }
}


int create_joinable_thread(pthread_t* thread, void* (*start_func)(void*), void* arg)
{
    int result;
    pthread_attr_t attr;
    pthread_t newThread;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    result = pthread_create(&newThread, &attr, start_func, arg);
    if (result == 0)
    {
        *thread = newThread;
    }
    else
    {
        ml_log_error("Failed to create joinable thread: %s\n", strerror(result));
    }

    pthread_attr_destroy(&attr);

    return result == 0;
}

void cancel_and_join_thread(pthread_t* thread, void* data, void (*stop_func)(void*))
{
    int result;
    void* status;

    if (*thread != 0)
    {
        if (stop_func != NULL)
        {
            stop_func(data);
        }

        if ((result = pthread_cancel(*thread)) != 0)
        {
            ml_log_warn("Failed to cancel thread: %s\n", strerror(result));
        }

        if ((result = pthread_join(*thread, (void **)&status)) != 0)
        {
            ml_log_warn("Failed to join thread: %s\n", strerror(result));
        }
        *thread = 0;
    }
}

void join_thread(pthread_t* thread, void* data, void (*stop_func)(void*))
{
    int result;
    void* status;

    if (*thread != 0)
    {
        if (stop_func != NULL)
        {
            stop_func(data);
        }

        if ((result = pthread_join(*thread, (void **)&status)) != 0)
        {
            ml_log_warn("Failed to join thread: %s\n", strerror(result));
        }
        *thread = 0;
    }
}


static void draw_vitc_line(unsigned char value[9], unsigned char *line)
{
	// Modelled on digibeta output where it was found:
	// * peak or trough of bit is always 5 pixels wide
	// * start of first sync bit is pixel 25

	// Draw transition from low to high for first sync bit
	int pos = 25;
	line[pos++ *2+1] = 0x2A;
	line[pos++ *2+1] = 0x68;
	line[pos++ *2+1] = 0xA6;
	// position is now at start of first sync bit peak (pos==28)

	int val_i, i;
	for (val_i = 0; val_i < 9; val_i++) {
		unsigned char val = value[val_i];
		//printf("drawing value %d at pixel pos %d (val_i=%d)\n", val, pos, val_i);

		// loop through 10 bits representing the byte (2 bits sync + 8 bits data)
		for (i = 0; i < 10; i++) {
			int even_bit = (i % 2) == 0;

			int bit, next_bit;
			if (i == 0) {		// handle sync bit pair specially
				bit = 1;
				next_bit = 0;
			}
			else if (i == 1) {
				bit = 0;
				next_bit = val & 1;
			}
			else {				// normal bit
				bit = (val >> (i-2)) & 1;
				// To calculate next_bit check whether we are at the end of the byte
				// in which case it will always be 1 (from sync bit) or 0 at end of the line
				if (val_i == 8)		// end of the line (CRC byte)?
					next_bit = ((i-2) == 7) ? 0 : (val >> ((i-2)+1)) & 1;
				else
					next_bit = ((i-2) == 7) ? 1 : (val >> ((i-2)+1)) & 1;
			}

			//printf("  i=%d %s b=%d nb=%d\n", i, even_bit?"even":"odd ", bit, next_bit);

			if (even_bit) {
				// draw 7 pixels for even bit
				if (bit) {
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					if (next_bit) {				// keep high
						line[pos++ *2+1] = 0xC0;
						line[pos++ *2+1] = 0xC0;
					}
					else {						// transition to low
						line[pos++ *2+1] = 0x94;
						line[pos++ *2+1] = 0x3C;
					}
				}
				else {	// low bit
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					if (next_bit) {				// transition to high
						line[pos++ *2+1] = 0x3C;
						line[pos++ *2+1] = 0x94;
					}
					else {						// keep low
						line[pos++ *2+1] = 0x10;
						line[pos++ *2+1] = 0x10;
					}
				}
			}
			else {
				// draw 8 pixels for odd bit
				if (bit) {
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					line[pos++ *2+1] = 0xC0;
					if (next_bit) {				// keep high
						line[pos++ *2+1] = 0xC0;
						line[pos++ *2+1] = 0xC0;
						line[pos++ *2+1] = 0xC0;
					}
					else {						// transition to low
						line[pos++ *2+1] = 0xA6;
						line[pos++ *2+1] = 0x68;
						line[pos++ *2+1] = 0x2A;
					}
				}
				else {	// low bit
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					line[pos++ *2+1] = 0x10;
					if (next_bit) {				// transition to high
						line[pos++ *2+1] = 0x2A;
						line[pos++ *2+1] = 0x68;
						line[pos++ *2+1] = 0xA6;
					}
					else {						// keep low
						line[pos++ *2+1] = 0x10;
						line[pos++ *2+1] = 0x10;
						line[pos++ *2+1] = 0x10;
					}
				}
			}
		}
	}
}

static void write_vitc_one_field(unsigned hh, unsigned mm, unsigned ss, unsigned ff, int field_flag, unsigned char *line)
{
	// Set line to black UYVY
	int i;
	for (i = 0; i < 720; i++) {
		line[i*2+0] = 0x80;
		line[i*2+1] = 0x10;
	}

	// data for one line is 9 bytes (8 bytes + 1 byte CRC)
	unsigned char data[9] = {0};

	int di = 0;
	data[di++] = ff % 10;
	data[di++] = ff / 10;
	data[di++] = ss % 10;
	data[di++] = ss / 10;
	data[di++] = mm % 10;
	data[di++] = mm / 10;
	data[di++] = hh % 10;
	data[di++] = (hh / 10) | (field_flag << 3);

	//
	// calculate CRC
	//

	// create 64bt value representing 8 * 8 bits
	unsigned long long alldata = 0;
	for (i = 0; i < 8; i++) {
		alldata |= ((unsigned long long)data[i]) << (i*8);
	}
	// convert array of 8 data bytes into array of 10 values including sync bits (2 bits)
	unsigned char data10[10];
	int data10i = 0;
	int output = 0;
	int out_i = 0;
	for (i = 0; i < 8*8; i++) {
		int bit = (alldata >> i) & 0x1;
		if (i % 8 == 0) {
			// add sync bits (1 0)
			output |= 1 << (out_i++);
			output |= 0 << (out_i++);
			if (out_i == 8) {
				data10[data10i++] = output;
				out_i = 0;
				output = 0;
			}
		}
		output |= bit << (out_i++);

		if (out_i == 8) {
			data10[data10i++] = output;
			out_i = 0;
			output = 0;
		}
	}

	// Now compute the CRC by XORing 10 x 8bits
	int CRC = 0;
	for (i = 0; i < 10; i++) {
		CRC ^= data10[i];
	}
	CRC = (((CRC & 3) ^ 1) << 6) | (CRC >> 2);

	data[di++] = CRC;

	draw_vitc_line(data, line);
}

void writeVITC(unsigned hh, unsigned mm, unsigned ss, unsigned ff, unsigned char *line)
{
	write_vitc_one_field(hh, mm, ss, ff, 0, line);
	write_vitc_one_field(hh, mm, ss, ff, 1, line + 720*2);
}


double calc_audio_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power)
{
    int i;
    double squareSum = 0.0;
    double max_audio_level = 1.0;
    int8_t sample8;
    int16_t sample16;
    int32_t sample32;
    double power;

    switch (byte_alignment)
    {
        case 1:
            for (i = 0; i < num_samples; i++)
            {
                sample8 = (int8_t)p_samples[i];

                squareSum += (double)sample8 * (double)sample8;
            }
            max_audio_level = 128; /* max signed 8-bit */
            break;
        case 2:
            for (i = 0; i < num_samples; i++)
            {
                sample16 = (int16_t)(
                    (uint16_t)p_samples[i * 2] |
                    (((uint16_t)p_samples[i * 2 + 1]) << 8));

                squareSum += (double)sample16 * (double)sample16;
            }
            max_audio_level = 32768; /* max signed 16-bit */
            break;
        case 3:
            for (i = 0; i < num_samples; i++)
            {
                sample32 = (int32_t)(
                    (((uint32_t)p_samples[i * 3]) << 8) |
                    (((uint32_t)p_samples[i * 3 + 1]) << 16) |
                    (((uint32_t)p_samples[i * 3 + 2]) << 24));

                squareSum += (double)sample32 * (double)sample32;
            }
            max_audio_level = 2.0*1024*1024*1024; /* max signed 32-bit */
            break;
        case 4:
            for (i = 0; i < num_samples; i++)
            {
                sample32 = (int32_t)(
                    (uint32_t)p_samples[i * 4] |
                    (((uint32_t)p_samples[i * 4 + 1]) << 8) |
                    (((uint32_t)p_samples[i * 4 + 2]) << 16) |
                    (((uint32_t)p_samples[i * 4 + 3]) << 24));

                squareSum += (double)sample32 * (double)sample32;
            }
            max_audio_level = 2.0*1024*1024*1024; /* max signed 32-bit */
            break;
        default:
            /* not supported */
            squareSum = 0.0;
            max_audio_level = 1.0;
            break;
    }

    /* return min_power - 1 if no audio is present */
    if (squareSum <= 0.0)
    {
        return min_power - 1;
    }

    /* dBFS - decibel full scale */
    /* 20 * log10(sqrt(squareSum / num_samples) / max_audio_level) dBFS */

    power = 10 * log10(squareSum / ((double)num_samples)) - 20 * log10(max_audio_level);

    /* always return min_power if there is audio */
    if (power < min_power)
    {
        return min_power;
    }

    return power;
}

double calc_audio_peak_power(const unsigned char* p_samples, int num_samples, int byte_alignment, double min_power)
{
    int i;
    int32_t sample;
    int32_t maxSample = 0;
    double power;

    switch (byte_alignment)
    {
        case 1:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)((uint32_t)p_samples[i] << 24);

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 2:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 2]) << 16) |
                    (((uint32_t)p_samples[i * 2 + 1]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 3:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (((uint32_t)p_samples[i * 3]) << 8) |
                    (((uint32_t)p_samples[i * 3 + 1]) << 16) |
                    (((uint32_t)p_samples[i * 3 + 2]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        case 4:
            for (i = 0; i < num_samples; i++)
            {
                sample = (int32_t)(
                    (uint32_t)p_samples[i * 4] |
                    (((uint32_t)p_samples[i * 4 + 1]) << 8) |
                    (((uint32_t)p_samples[i * 4 + 2]) << 16) |
                    (((uint32_t)p_samples[i * 4 + 3]) << 24));

                maxSample = (sample > maxSample) ? sample : maxSample;
                maxSample = (-sample > maxSample) ? -sample : maxSample;
            }
            break;
        default:
            /* not supported */
            break;
    }

    /* return min_power - 1 if no audio is present */
    if (maxSample == 0)
    {
        return min_power - 1;
    }

    /* dBFS - decibel full scale */
    power = 20 * log10(maxSample / (2.0*1024*1024*1024));

    /* always return min_power if there is audio */
    if (power < min_power)
    {
        return min_power;
    }

    return power;
}


void strcat_separator(char* path)
{
    size_t len = strlen(path);

    if (len == 0 || path[len - 1] != '/')
    {
        path[len] = '/';
        path[len + 1] = '\0';
    }
}

char* get_host_name(char* buffer, size_t bufferSize)
{
    if (gethostname(buffer, bufferSize - 1) != 0)
    {
        // failed
        buffer[0] = '\0';
        return buffer;
    }

    buffer[bufferSize - 1] = '\0';
    return buffer;
}

char* get_user_name(char* buffer, size_t bufferSize)
{
    FILE* cmdStdout = popen("whoami", "r");
    if (cmdStdout == 0)
    {
        // failed
        buffer[0] = '\0';
        return buffer;
    }

    size_t numRead = fread(buffer, 1, bufferSize - 1, cmdStdout);
    if (numRead <= 0)
    {
        // failed
        buffer[0] = '\0';
    }
    else
    {
        buffer[numRead] = '\0';

        // trim end
        int i;
        for (i = numRead - 1; i >= 0; i--)
        {
            if (!isspace(buffer[i]))
            {
                break;
            }

            buffer[i] = '\0';
        }
    }

    pclose(cmdStdout);

    return buffer;
}


int64_t convert_length(int64_t length, const Rational* oldFrameRate, const Rational* newFrameRate)
{
    double factor;
    int64_t newLength;

    if (length < 0 || memcmp(oldFrameRate, newFrameRate, sizeof(*oldFrameRate)) == 0 ||
        oldFrameRate->num < 1 || oldFrameRate->den < 1 || newFrameRate->num < 1 || newFrameRate->den < 1)
    {
        return length;
    }

    factor = newFrameRate->num * oldFrameRate->den / (double)(newFrameRate->den * oldFrameRate->num);
    newLength = (int64_t)(length * factor + 0.5);

    if (newLength < 0)
    {
        /* e.g. if length was the max value and the new rate is faster */
        return length;
    }

    return newLength;
}

int64_t convert_non_drop_timecode(int64_t timecode, const Rational* oldFrameRate, const Rational* newFrameRate)
{
    double factor;
    int64_t newTimecode;

    if (timecode < 0 || memcmp(oldFrameRate, newFrameRate, sizeof(*oldFrameRate)) == 0 ||
        oldFrameRate->num < 1 || oldFrameRate->den < 1 || newFrameRate->num < 1 || newFrameRate->den < 1)
    {
        return timecode;
    }

    factor = get_rounded_frame_rate(newFrameRate) / (double)get_rounded_frame_rate(oldFrameRate);
    newTimecode = (int64_t)(timecode * factor + 0.5);

    if (newTimecode < 0)
    {
        /* e.g. if timecode was the max value and the new rate is faster */
        return timecode;
    }

    return newTimecode;
}



