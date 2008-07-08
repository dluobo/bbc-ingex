/*
 * $Id: capture.cpp,v 1.1 2008/07/08 16:22:29 philipn Exp $
 *
 * Read frames from the DVS SDI capture card, write to an MXF file, generate a 
 * browse copy file and perform PSE analysis  
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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
    A capture starts when start_record() is called. A capture_thread is running 
    all the time and it starts the DVS card's FIFO buffer and reads the frames 
    after the start_record() call.
    
    A store_video_thread reads the next frame from a ring buffer, which includes
    video, audio and timecode information, and writes it to the MXF file. At the
    same time the video data is analysed for PSE failures.
    
    A browse copy generation thread, store_browse_thread, is started when 
    start_record is called. This generate a MPEG-2 browse copy file and also
    write timecodes for each frame to a text file. It can happen that the browse
    generation cannot keep up, eg. after a couple of seconds of noisy video,
    and if this happens the encoder encodes black frames instead until it has 
    caught up.
    
    A stop_record() call stops the capture and the MXF file writing is completed 
    with the D3 Infax data, PSE failures and VTR errors. The browse copy 
    generation will normally complete shortly thereafter and the 
    store_browse_thread is stopped.
    
    The start_multi_item_record() and stop_multi_item_record() methods are used 
    for multi-item D3 tapes where the initial MXF file is only temporary. This 
    MXF file is written in segments to minimise the disk space requirements when 
    chunking the file into the individual MXF files for each item.
    
    A capture can be abort()ed at any time. A capture is also aborted if the SDI 
    signal is disrupted or when the store_video_thread cannot keep up, eg. 
    writing to disk slows or stops when the disk is full.
    
    
    The DVS FIFO is not running all the time because this results in A/V 
    synchronization errors when the SDI signal is disrupted, eg. when setting 
    the source VTR into play mode. The capture thread only starts the FIFO and
    reads frames once the VTR is playing and the SDI signal is stable. Use
    set_debug_avsync() to test A/V sync using a clapperboard sequence.
    
    The original design played out the captured data through the DVS card's 
    SDI output for monitoring purposes. However, this would sometimes result in 
    disruptions in the video signal. Instead the default DVS card loopthrough 
    signal on the SDI output is used for monitoring.
    
    The system has been tested using a Centaurus DVS card and SDK version 
    2.7p57. It has not been tested with later cards, eg. Centaurus II cards or
    later SDK versions which are said not to have the A/V synchronization 
    issues when the SDI signal is disrupted.
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <libgen.h>

#include <signal.h>
#include <unistd.h> 
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

#include <pthread.h>
#include <string>

#include <stdarg.h>
#include <mxf/mxf_logging.h>    // to redirect mxf logs

#include "capture.h"
#include "logF.h"
#include "video_VITC.h"
#include "video_conversion.h"
#include "video_test_signals.h"
#include "avsync_analysis.h"
#include "PSEReport.h"
#include "BrowseEncoder.h"

#include <mxf/mxf_page_file.h>

#ifdef HAVE_PSE_FPA
#include "pse_fpa.h"
#else
#include "pse_simple.h"
#endif


// make sure we are compiling and linking with DVS SDK version 2.7p57
#if !(DVS_VERSION_MAJOR == 2 && DVS_VERSION_MINOR == 7 && DVS_VERSION_PATCH == 57)
#error Capture code requires DVS SDK version 2.7p57.
#endif


using std::string;
using std::make_pair;

#define VIDEO_FRAME_WIDTH       720
#define VIDEO_FRAME_HEIGHT      576
#define VIDEO_FRAME_SIZE        (VIDEO_FRAME_WIDTH * VIDEO_FRAME_HEIGHT * 2)
#define AUDIO_FRAME_SIZE        1920 * 3        // 1 channel, 48000Hz sample rate at 24bits per sample

#define CHK(x) {if (! (x)) { logTF("failed at %s line %d\n", __FILE__, __LINE__); } }


// helper class which remembers to suspend capture if something fails
class CaptureSuspendGuard
{
public:
    CaptureSuspendGuard(bool* suspend)
    : _suspend(suspend), _confirmed(false)
    {
        *_suspend = false;
    }
    
    ~CaptureSuspendGuard()
    {
        if (!_confirmed)
        {
            // suspend capture
            *_suspend = true;
        }
    }
    
    void confirmStatus()
    {
        // capture can remain not suspended
        _confirmed = true;
    }
    
private:
    bool* _suspend;
    bool _confirmed;
};


// helper class to signal when a thread ends
class ThreadRunningGuard
{
public:
    ThreadRunningGuard(bool* isRunning)
    : _isRunning(isRunning)
    {
        *_isRunning = true;
    }
    
    ~ThreadRunningGuard()
    {
        *_isRunning = false;
    }
    
private:
    bool* _isRunning;
};

static void sleep_msec(int64_t msec)
{
    struct timespec rem;
    struct timespec tm = {msec / 1000, (msec % 1000) * 1000000};
    
    int result;
    while (true)
    {
        result = clock_nanosleep(CLOCK_MONOTONIC, 0, &tm, &rem);
        if (result != 0 && errno == EINTR)
        {
            // sleep was interrupted by a signal - sleep for the remaining time
            tm = rem;
            continue;
        }
        
        break;
    }
}

static bool dir_exists(const char *dir)
{
    struct stat buf;
    if (stat(dir, &buf) != 0)
        return false;

    if (S_ISDIR(buf.st_mode))
        return true;
    else
        return false;
}

// Trap compiles which do not use 64bit filesizes
#if (_FILE_OFFSET_BITS < 64)
#error not using _FILE_OFFSET_BITS >= 64
#endif

static uint64_t get_filesize(const char *file)
{
    if (file == 0)
        return 0;
    
    struct stat buf;
    if (stat(file, &buf) != 0)
        return 0;

    return buf.st_size;
}

// Redirect MXF log messages to local log function
// Enabled by assigning to extern variable mxf_log
void redirect_mxf_logs(MXFLogLevel level, const char* format, ...)
{
    va_list     ap;

    va_start(ap, format);

    const char *level_str = "Unknown";
    switch (level)
    {
        case MXF_DLOG:
            level_str = "Debug";
            break;            
        case MXF_ILOG:
            level_str = "Info";
            break;            
        case MXF_WLOG:
            level_str = "Warning";
            break;            
        case MXF_ELOG:
            level_str = "ERROR";
            break;            
    };
    logTF("MXF %s: \n", level_str);
    vlogTF(format, ap);

    va_end(ap);
}

static void dvsaudio32_to_24bitmono(int channel, uint8_t *buf32, uint8_t *buf24)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    int channel_offset = 0;
    if (channel == 1)
        channel_offset = 4;

    // Skip every other channel, copying 24 most significant bits of 32 bits
    // from little-endian DVS format to little-endian 24bits
    for (i = channel_offset; i < 1920*4*2; i += 8) {
        *buf24++ = buf32[i+1];
        *buf24++ = buf32[i+2];
        *buf24++ = buf32[i+3];
    }
}

static void dvsaudio32_to_16bitstereo(uint8_t *buf32, int16_t *buf16)
{
    int i;
    // A DVS audio buffer contains a mix of two 32bits-per-sample channels
    // Data for one sample pair is 8 bytes:
    //  a0 a0 a0 a0  a1 a1 a1 a1

    for (i = 0; i < 1920*4*2; i += 8) {
        *buf16++ = (((uint16_t)buf32[i+3]) << 8) | buf32[i+2];
        *buf16++ = (((uint16_t)buf32[i+7]) << 8) | buf32[i+6];
    }
}

static void int_to_timecode(int tc_as_int, ArchiveTimecode *p)
{
    if (tc_as_int == -1) {      // invalid timecode, set all values to zero
        p->frame = 0;
        p->hour = 0;
        p->min = 0;
        p->sec = 0;
        return;
    }

    p->frame = tc_as_int % 25;
    p->hour = (int) (tc_as_int / (60 * 60 * 25));
    p->min = (int) ((tc_as_int - (p->hour * 60 * 60 * 25)) / (60 * 25));
    p->sec = (int) ((tc_as_int - (p->hour * 60 * 60 * 25) - (p->min * 60 * 25)) / 25);
}

static void write_browse_timecode(FILE* tcFile, ArchiveTimecode ctc, ArchiveTimecode vitc, ArchiveTimecode ltc)
{
    fprintf(tcFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
        ctc.hour, ctc.min, ctc.sec, ctc.frame,
        vitc.hour, vitc.min, vitc.sec, vitc.frame,
        ltc.hour, ltc.min, ltc.sec, ltc.frame);
}

static const char *videomode2str(VideoMode videomode)
{
    if (videomode == PAL) {
        return "PAL";
    }
    else if (videomode == PALFF) {
        return "PALFF";
    }
    else if (videomode == NTSC) {
        return "NTSC";
    }
    else if (videomode == HD1080i50) {
        return "HD1080i50";
    }
    return "unknown";
}

static bool check_videomode(sv_handle *sv, VideoMode videomode)
{
    sv_info status_info;
    sv_status(sv, &status_info);
    int width = status_info.xsize;
    int height = status_info.ysize;

    if (videomode == PAL) {
        if (width != 720 || height != 576)
            return false;
    }
    else if (videomode == PALFF) {
        if (width != 720 || height != 592)
            return false;
    }
    else if (videomode == NTSC) {
        if (width != 720 || height != 486)
            return false;
    }
    else if (videomode == HD1080i50) {
        if (width != 1920 || height != 1080)
            return false;
    }
    else {
        logFF("Unknown video mode");
        return false;
    }
    return true;
}

static bool set_videomode(sv_handle *sv, VideoMode videomode)
{
    int mode;
    if (videomode == PAL) {
        mode = SV_MODE_PAL;
    }
    else if (videomode == PALFF) {
        mode = SV_MODE_PALFF;
    }
    else if (videomode == NTSC) {
        mode = SV_MODE_NTSC;
    }
    else if (videomode == HD1080i50) {
        mode = SV_MODE_SMPTE274_25I;
    }
    else {
        logFF("Unknown video mode");
        return false;
    }
    
    mode &= ~SV_MODE_COLOR_MASK;
    mode |=  SV_MODE_COLOR_YUV422;

    mode |=  SV_MODE_STORAGE_FRAME;

    mode &= ~SV_MODE_AUDIO_MASK;
    mode |=  SV_MODE_AUDIO_8CHANNEL;

    mode &= ~SV_MODE_AUDIOBITS_MASK;
    mode |=  SV_MODE_AUDIOBITS_32;

    // mode should be 1080033280 for PALFF, 1080033280 for PAL
    int res = sv_videomode(sv, mode);
    if (res != SV_OK) {
        logFF("Set video mode to %s: sv_videomode( %d (0x%08x)) (res=%d)\n", videomode2str(videomode), mode, mode, res);
        return false;
    }

    return true;
}

void Capture::set_loglev(int level)
{
    loglev = level;
}

int Capture::inc_loglev(void)
{
    loglev++;
    return loglev;
}

int Capture::dec_loglev(void)
{
    loglev--;
    return loglev;
}

int Capture::get_loglev(void)
{
    return loglev;
}

void Capture::set_mxf_page_size(int64_t size)
{
    mxf_page_size = size;
}

void Capture::set_num_audio_tracks(int num)
{
    if (num < 0)
    {
        logTF("Error: duh, cannot capture negative number of audio streams\n");
        return;
    }
    if (num > MAX_CAPTURE_AUDIO)
    {
        logTF("Error: cannot capture %d audio streams; maximum is %d\n", num, MAX_CAPTURE_AUDIO);
        return;
    }
    
    num_audio_tracks = num;
}

void Capture::set_browse_enable(bool enable)
{
    browse_enabled = enable;
}

void Capture::set_browse_video_bitrate(int browsekbps)
{
    browse_kbps = browsekbps;
}

void Capture::set_browse_thread_count(int count)
{
    browse_thread_count = count;
}

void Capture::set_browse_overflow_frames(int browseOverflowFrames)
{
    browse_overflow_frames = browseOverflowFrames;
}

void Capture::set_pse_enable(bool enable)
{
    pse_enabled = enable;
}

void Capture::set_vitc_lines(int* lines, int num_lines)
{
    if (num_lines > MAX_VBI_LINES)
    {
        logTF("Error: number of VITC lines %d exceeds maximum %d\n", num_lines, MAX_VBI_LINES);
        return;
    }
    
    int i;
    for (i = 0; i < num_lines; i++)
    {
        if (lines[i] < 15 || lines[i] > 22)
        {
            logTF("Warning: VITC line %d is outside the PAL-FF VBI (15 <= VBI <= 22)\n", lines[i]);
        }
        vitc_lines[i] = lines[i];
    }
    num_vitc_lines = num_lines;
}

void Capture::set_ltc_lines(int* lines, int num_lines)
{
    if (num_lines > MAX_VBI_LINES)
    {
        logTF("Error: number of LTC lines %d exceeds maximum %d\n", num_lines, MAX_VBI_LINES);
        return;
    }
    
    int i;
    for (i = 0; i < num_lines; i++)
    {
        if (lines[i] < 15 || lines[i] > 22)
        {
            logTF("Warning: LTC line %d is outside the PAL-FF VBI (15 <= VBI <= 22)\n", lines[i]);
        }
        ltc_lines[i] = lines[i];
    }
    num_ltc_lines = num_lines;
}

void Capture::set_debug_avsync(bool enable)
{
    debug_clapper_avsync = enable;
}

void Capture::set_debug_vitc_failures(bool enable)
{
    debug_vitc_failures = enable;
}

void Capture::set_debug_store_thread_buffer(bool enable)
{
    debug_store_thread_buffer = enable;
}


uint8_t *Capture::rec_ring_frame(int frame)
{
    return rec_ring + (frame % ring_buffer_size) * element_size;
}
int Capture::ring_element_size(void)
{
    return element_size;
}
int Capture::ring_audio_pair_offset(int channelPair)
{
    return audio_pair_offset[channelPair];
}
int Capture::ring_vitc_offset(void)
{
    return element_size - sizeof(int) * 1;
}
int Capture::ring_ltc_offset(void)
{
    return element_size - sizeof(int) * 2;
}

int Capture::last_captured(void)
{
    pthread_mutex_lock(&m_last_frame_captured);
    int last = g_last_frame_captured;
    pthread_mutex_unlock(&m_last_frame_captured);
    return last;
}

void Capture::signal_frame_ready()
{
    pthread_mutex_lock(&m_frame_ready_cond);
    pthread_cond_broadcast(&frame_ready_cond);
    pthread_mutex_unlock(&m_frame_ready_cond);
}

void Capture::update_last_captured(int update)
{
    pthread_mutex_lock(&m_last_frame_captured);
    g_last_frame_captured = update;
    pthread_mutex_unlock(&m_last_frame_captured);

    signal_frame_ready();
}

void Capture::wait_for_frame_ready(void)
{
    // wait a maximum of 2 seconds
    struct timespec endtime;
    clock_gettime(CLOCK_MONOTONIC, &endtime);
    endtime.tv_sec += 2;
    
    pthread_mutex_lock(&m_frame_ready_cond);
    pthread_cond_timedwait(&frame_ready_cond, &m_frame_ready_cond, &endtime);
    pthread_mutex_unlock(&m_frame_ready_cond);
}

int Capture::last_written(void)
{
    pthread_mutex_lock(&m_last_frame_written);
    int last = g_last_frame_written;
    pthread_mutex_unlock(&m_last_frame_written);
    return last;
}
void Capture::update_last_written(int update)
{
    pthread_mutex_lock(&m_last_frame_written);
    g_last_frame_written = update;
    pthread_mutex_unlock(&m_last_frame_written);
}

void Capture::get_general_stats(GeneralStats *p)
{
    p->video_ok = video_ok;
    p->audio_ok = audio_ok;
    p->recording = g_record_start;
}

void Capture::get_record_stats(RecordStats *p)
{
    // STARTED state not set here
    if (g_record_start)
        p->record_state = IN_PROGRESS;
    else if (g_aborted)
        p->record_state = FAILED;
    else
        p->record_state = COMPLETED;

    int_to_timecode(start_vitc, &p->start_vitc);
    int_to_timecode(start_ltc, &p->start_ltc);
    int_to_timecode(current_vitc, &p->current_vitc);
    int_to_timecode(current_ltc, &p->current_ltc);

    p->current_framecount = g_frames_written;
    pthread_mutex_lock(&m_mxfout);
    if (mxfout != 0)
    {
        // use the archive mxf 'class' because multi-item sources will
        // result in mxf page files being used which are split of 1 or more files
        p->file_size = get_archive_mxf_file_size(mxfout);
    }
    else
    {
        p->file_size = 0;
    }
    pthread_mutex_unlock(&m_mxfout);
    p->browse_file_size = get_filesize(browse_filename);
    p->materialPackageUID = g_materialPackageUID;
    p->filePackageUID = g_filePackageUID;
    p->tapePackageUID = g_tapePackageUID;
}

void Capture::set_recording_status(bool recording_state)
{
    if (recording_state != recording_ok)
    {
        recording_ok = recording_state;
    
        if (_listener) {
            _listener->sdiStatusChanged(video_ok, audio_ok, recording_ok);
        }
    }
}

void Capture::set_input_SDI_status(bool video_state, bool audio_state)
{
    if (video_state != video_ok || audio_state != audio_ok)
    {
        video_ok = video_state;
        audio_ok = audio_state;
    
        if (_listener) {
            _listener->sdiStatusChanged(video_ok, audio_ok, recording_ok);
        }
    }
}

void Capture::report_mxf_buffer_overflow()
{
    if (_listener) {
        _listener->mxfBufferOverflow();
    }
}

void Capture::report_browse_buffer_overflow()
{
    if (_listener) {
        _listener->browseBufferOverflow();
    }
}

int Capture::read_timecode(uint8_t* video_data, int stride, int line, const char* type_str)
{
    unsigned hh, mm, ss, ff;
    uint8_t* vbiline;
    int palff_line = line - 15; // PAL_FF video starts at line 15
    
    if (palff_line >= 0)
    {
        vbiline = video_data + stride * palff_line * 2;
        if (readVITC(vbiline, &hh, &mm, &ss, &ff)) {
            if (loglev > 1) logFFi("%s (%d): %02u:%02u:%02u:%02u", type_str, line, hh, mm, ss, ff);
            return tc_to_int(hh, mm, ss, ff);
        }
        else {
            // Log failure only if line is interesting
            if (! black_or_grey_line(vbiline, 720)) {
                if (debug_vitc_failures) save_video_line(palff_line * 2, vbiline);
                if (loglev > 1) logFFi("%s (%d) failed", type_str, line);
            }
        }
    }
    
    return -1;
}



extern void *capture_video_thread_wrapper(void *p_obj)
{
    Capture *p = (Capture *)(p_obj);
    p->capture_video_thread();
    return NULL;
}

extern void *store_video_thread_wrapper(void *p_obj)
{
    Capture *p = (Capture *)(p_obj);
    p->store_video_thread();
    return NULL;
}

extern void *store_browse_thread_wrapper(void *p_obj)
{
    Capture *p = (Capture *)(p_obj);
    p->store_browse_thread();
    return NULL;
}

extern void *encode_browse_thread_wrapper(void *p_obj)
{
    Capture *p = (Capture *)(p_obj);
    p->encode_browse_thread();
    return NULL;
}

bool Capture::start_record(const char *filename, const char *browseFilename, const char *browseTimecodeFilename, 
    const char *pseReportFilename)
{
    logFF("start_record(filename=%s, browseFilename=%s, browseTimecodeFilename=%s, pseReportFilename=%s)\n",
        filename, browseFilename, browseTimecodeFilename, pseReportFilename);
    logFF("browse_enabled=%s; pse_enabled=%s\n", browse_enabled ? "true" : "false", pse_enabled ? "true" : "false"); 

    if (g_record_start) {           // already started recording
        logTF("start_record: FAILED - recording in progress\n");
        return false;
    }

    if (browse_enabled) {
        if (browse_thread_running) {
            logTF("Failed to prepare browse generation whilst existing process is still busy\n");
            return false;
        }
    }

    // reset the stop frame after we know the browse thread is not running 
    g_record_stop_frame = -2;
    
    
    // take capture out of suspend and re-suspend if something fails further below
    int prev_last_frame_captured = last_captured();
    CaptureSuspendGuard captureSuspendGuard(&g_suspend_capture);
    
    // wait for first frame to be captured
    int count = 100; // wait maximum 1 second for first frame to be captured
    while (last_captured() <= prev_last_frame_captured && count > 0)
    {
        sleep_msec(10);
        count--;
    }
    if (count == 0)
    {
        logTF("start_record: failed to start capturing within 1 second of starting\n");
        return false;
    }
    
    // For start 'now' operation, record current frame now
    int last_frame_captured = last_captured();
    logFF("start_record: last_frame_captured=%d\n", last_frame_captured);

    // We don't check for input video status since it is ok to
    // start recording without a good signal, but the recording will
    // fail if we don't get a good signal soon

    // store mxf and browse filenames
    strcpy(mxf_filename, filename);
    strcpy(browse_filename, browseFilename);
    strcpy(browse_timecode_filename, browseTimecodeFilename);

    // make MXF directory if not present
    int res;
    char tmppath[FILENAME_MAX];
    strcpy(tmppath, mxf_filename);
    const char *mxfdir = dirname(tmppath);
    if (! dir_exists(mxfdir)) {
        if ((res = mkdir(mxfdir, 0775)) != 0) {
            logTF("start_record: mkdir(%s) failed, res=%d (%s)\n", mxfdir, res, strerror(errno));
            return false;
        }
    }

    // Open MXF file for writing
    pthread_mutex_lock(&m_mxfout);
    res = prepare_archive_mxf_file(mxf_filename, num_audio_tracks, 0, strict_MXF_checking, &mxfout);
    pthread_mutex_unlock(&m_mxfout);
    if (!res) {
        logTF("Failed to prepare MXF writer\n");
        return false;
    }

    // get the package UIDs
    g_materialPackageUID = get_material_package_uid(mxfout);
    g_filePackageUID = get_file_package_uid(mxfout);
    g_tapePackageUID = get_tape_package_uid(mxfout);


    // Crash record: start from last captured frame
    int first_frame_to_write;
    first_frame_to_write = last_frame_captured;
    update_last_written(first_frame_to_write - 1);
    logFF("start_record: first_frame_to_write=%d\n", first_frame_to_write);

    // Setup new browse encoding
    if (browse_enabled) {

        // Indicate first browse frame to be written (last + 1 = first -> last = first - 1)
        g_last_browse_frame_written = first_frame_to_write - 1;

        // initializations
        int i;
        for (i = 0; i < 2; i++)
        {
            browse_frames[i].readyToRead = false;
            browse_frames[i].isEndOfSequence = false;
            browse_frames[i].isBlackFrame = false;
        }
        last_browse_frame_write = 0;
        last_browse_frame_read = 0;
        

        // create browse store thread and browse encode thread
        
        pthread_t   browse_thread;
        if ((res = pthread_create(&browse_thread, NULL, store_browse_thread_wrapper, this)) != 0) {
            logTF("Failed to create browse thread: %s\n", strerror(res));
            return false;
        }
        logTF("browse thread id = %lu\n", browse_thread);

        pthread_t   encode_browse_thread;
        if ((res = pthread_create(&encode_browse_thread, NULL, encode_browse_thread_wrapper, this)) != 0) {
            logTF("Failed to create encode browse thread: %s\n", strerror(res));
            return false;
        }
        logTF("encode browse thread id = %lu\n", browse_thread);
    }

    // record start vitc and ltc (for record stats)
    uint8_t *addr = rec_ring_frame(first_frame_to_write);
    memcpy(&start_ltc, addr + ring_ltc_offset(), sizeof(start_ltc));
    memcpy(&start_vitc, addr + ring_vitc_offset(), sizeof(start_vitc));
    logFF("start_ltc=%d start_vitc=%d\n", start_ltc, start_vitc);

    set_recording_status(true);
    g_frames_written = 0;
    g_browse_written = 0;
    g_aborted = false;

    // Open a new PSE analysis
    if (pse_enabled)
    {
        pse_report = pseReportFilename;
        pse->open();
    }

    // Clear timecodes list
    timecodes.clear();

    // signal start recording
    g_record_start = true;
    
    captureSuspendGuard.confirmStatus(); // everything went ok, so let capture continue

    return true;
}

bool Capture::stop_record(int duration, InfaxData* infaxData, VTRError *vtrErrors, int numVTRErrors, int* pseResult)
{
    *pseResult = -1; // only set to 0 or 1 later on if the analysis was completed
    
    if (! g_record_start) {
        logFF("stop_record: FAILED - recording not in progress\n");
        return false;
    }

    // duration of -1 means stop now (this instant)
    if (duration == -1) {
        g_record_stop_frame = last_captured();
    }
    else {
        g_record_stop_frame = g_record_start_frame + duration - 1;
    }

    logFF("stop_record: g_record_stop_frame = %d (duration = %d)\n", g_record_stop_frame, duration);

    // wait until recording stopped before returning
    while (g_record_start) {
        // wake up the store threads
        signal_frame_ready();
        
        sleep_msec(20);
    }

    // suspend the capture thread
    g_suspend_capture = true;

    // Get all PSE results
    int numPSEFailues = 0;
    PSEFailure *pseFailures = 0;
    if (pse_enabled)
    {
        std::vector<PSEResult> results;
        pse->get_remaining_results(results);
        pse->close();
    
        // Convert PSE results to C array for MXF call
        numPSEFailues = results.size();
        logFF("PSE failures = %d\n", numPSEFailues);
        pseFailures = new PSEFailure[numPSEFailues];
        for (int i = 0; i < numPSEFailues; i++) {
            pseFailures[i].position = results[i].position;
            pseFailures[i].vitcTimecode = timecodes[pseFailures[i].position].first;
            pseFailures[i].ltcTimecode = timecodes[pseFailures[i].position].second;
            pseFailures[i].redFlash = results[i].red;
            pseFailures[i].luminanceFlash = results[i].flash;
            pseFailures[i].spatialPattern = results[i].spatial;
            pseFailures[i].extendedFailure = results[i].extended;
            logFF("    %4d: pos=%6llu VITC=%02d:%02d:%02d:%02d LTC=%02d:%02d:%02d:%02d red=%4d fls=%4d spt=%4d ext=%d\n", i, pseFailures[i].position,
                    pseFailures[i].vitcTimecode.hour, pseFailures[i].vitcTimecode.min,
                    pseFailures[i].vitcTimecode.sec, pseFailures[i].vitcTimecode.frame,
                    pseFailures[i].ltcTimecode.hour, pseFailures[i].ltcTimecode.min,
                    pseFailures[i].ltcTimecode.sec, pseFailures[i].ltcTimecode.frame,
                    pseFailures[i].redFlash, pseFailures[i].luminanceFlash, pseFailures[i].spatialPattern, pseFailures[i].extendedFailure);
        }
    }

    // Complete MXF file
    pthread_mutex_lock(&m_mxfout);
    int res = complete_archive_mxf_file(&mxfout, infaxData, pseFailures, numPSEFailues, vtrErrors, numVTRErrors);
    if (!res) {
        logFF("Failed to complete writing Archive MXF file\n");
    }
    mxfout = 0;
    pthread_mutex_unlock(&m_mxfout);

    // Write PSE resport
    if (pse_enabled)
    {
        PSEReport* pseReport = PSEReport::open(pse_report);
        if (pseReport == 0)
        {
            logFF("Failed to open %s - no PSE report written\n", pse_report.c_str());
        }
        else
        {
            bool passed;
            bool result = pseReport->write(0, g_frames_written - 1, "", infaxData, pseFailures, numPSEFailues, &passed);
            if (result) 
            {
                logFF("generate_PSE_report() succeeded. file=%s\n", pse_report.c_str());
                *pseResult = passed ? 0 : 1;
            }
            else 
            {
                logFF("generate_PSE_report() failed. file=%s\n", pse_report.c_str());
            }

            delete pseReport;
        }

        delete [] pseFailures;
    }


    logFF("stop_record: finished, g_frames_written=%d\n", g_frames_written);
    return true;
}

bool Capture::start_multi_item_record(const char* filename)
{
    if (strstr(filename, "%d") == 0)
    {
        logTF("start_multi_item_record: invalid page filename '%s'\n", filename);
        return false;
    }
    
    logFF("start_multi_item_record(filename=%s)\n", filename);

    if (g_record_start) {           // already started recording
        logTF("start_multi_item_record: FAILED - recording in progress\n");
        return false;
    }

    // reset the stop frame after we know the browse thread is not running 
    g_record_stop_frame = -2;
    
    // take capture out of suspend and re-suspend if something fails further below
    int prev_last_frame_captured = last_captured();
    CaptureSuspendGuard captureSuspendGuard(&g_suspend_capture);
    
    // wait for first frame to be captured
    int count = 100; // wait maximum 1 second for first frame to be captured
    while (last_captured() <= prev_last_frame_captured && count > 0)
    {
        sleep_msec(10);
        count--;
    }
    if (count == 0)
    {
        logTF("start_record: failed to start capturing within 1 second of starting\n");
        return false;
    }
    
    // For start 'now' operation, record current frame now
    int last_frame_captured = last_captured();
    logFF("start_multi_item_record: last_frame_captured=%d\n", last_frame_captured);

    // We don't check for input video status since it is ok to
    // start recording without a good signal, but the recording will
    // fail if we don't get a good signal soon

    // store mxf and browse filenames
    strcpy(mxf_filename, filename);
    strcpy(browse_filename, "");
    strcpy(browse_timecode_filename, "");

    // make MXF directory if not present
    int res;
    char tmppath[FILENAME_MAX];
    strcpy(tmppath, mxf_filename);
    const char *mxfdir = dirname(tmppath);
    if (! dir_exists(mxfdir)) {
        if ((res = mkdir(mxfdir, 0775)) != 0) {
            logTF("start_multi_item_record: mkdir(%s) failed, res=%d (%s)\n", mxfdir, res, strerror(errno));
            return false;
        }
    }

    // Open MXF file for writing
    MXFPageFile* mxfPageFile;
    MXFFile* mxfFile;
    if (!mxf_page_file_open_new(mxf_filename, mxf_page_size, &mxfPageFile))
    {
        logTF("start_multi_item_record: Failed to open MXF page file\n");
        return false;
    }
    mxfFile = mxf_page_file_get_file(mxfPageFile);
    
    pthread_mutex_lock(&m_mxfout);
    res = prepare_archive_mxf_file_2(&mxfFile, mxf_filename, num_audio_tracks, 0, strict_MXF_checking, &mxfout);
    pthread_mutex_unlock(&m_mxfout);
    if (!res) {
        logTF("start_multi_item_record: Failed to prepare MXF writer\n");
        mxf_file_close(&mxfFile);
        return false;
    }

    // get the package UIDs
    g_materialPackageUID = get_material_package_uid(mxfout);
    g_filePackageUID = get_file_package_uid(mxfout);
    g_tapePackageUID = get_tape_package_uid(mxfout);

    // Crash record: start from last captured frame
    int first_frame_to_write;
    first_frame_to_write = last_frame_captured;
    update_last_written(first_frame_to_write - 1);
    logFF("start_multi_item_record: first_frame_to_write=%d\n", first_frame_to_write);

    // record start vitc and ltc (for record stats)
    uint8_t *addr = rec_ring_frame(first_frame_to_write);
    memcpy(&start_ltc, addr + ring_ltc_offset(), sizeof(start_ltc));
    memcpy(&start_vitc, addr + ring_vitc_offset(), sizeof(start_vitc));
    logFF("start_ltc=%d start_vitc=%d\n", start_ltc, start_vitc);

    set_recording_status(true);
    g_frames_written = 0;
    g_browse_written = 0;
    g_aborted = false;

    // Open a new PSE analysis
    if (pse_enabled)
    {
        pse_report = "";
        pse->open();
    }

    // Clear timecodes list
    timecodes.clear();

    // signal start recording
    g_record_start = true;
    
    captureSuspendGuard.confirmStatus(); // everything went ok, so let capture continue

    return true;
}

bool Capture::stop_multi_item_record(int duration, VTRError *vtrErrors, int numVTRErrors, int* pseResult)
{
    InfaxData infaxData;
    memset(&infaxData, 0, sizeof(InfaxData));

    *pseResult = -1; // only set to 0 or 1 later on if the analysis was completed
    
    if (! g_record_start) {
        logFF("stop_multi_item_record: FAILED - recording not in progress\n");
        return false;
    }

    // duration of -1 means stop now (this instant)
    if (duration == -1) {
        g_record_stop_frame = last_captured();
    }
    else {
        g_record_stop_frame = g_record_start_frame + duration - 1;
    }

    logFF("stop_multi_item_record: g_record_stop_frame = %d (duration = %d)\n", g_record_stop_frame, duration);

    // wait until recording stopped before returning
    while (g_record_start) {
        // wake up the store threads
        signal_frame_ready();
        
        sleep_msec(20);
    }

    // suspend the capture thread
    g_suspend_capture = true;

    // Get all PSE result
    int numPSEFailues = 0;
    PSEFailure *pseFailures = 0;
    if (pse_enabled)
    {
        std::vector<PSEResult> results;
        pse->get_remaining_results(results);
        pse->close();
    
        // Convert PSE results to C array for MXF call
        numPSEFailues = results.size();
        logFF("PSE failures = %d\n", numPSEFailues);
        pseFailures = new PSEFailure[numPSEFailues];
        for (int i = 0; i < numPSEFailues; i++) {
            pseFailures[i].position = results[i].position;
            pseFailures[i].vitcTimecode = timecodes[pseFailures[i].position].first;
            pseFailures[i].ltcTimecode = timecodes[pseFailures[i].position].second;
            pseFailures[i].redFlash = results[i].red;
            pseFailures[i].luminanceFlash = results[i].flash;
            pseFailures[i].spatialPattern = results[i].spatial;
            pseFailures[i].extendedFailure = results[i].extended;
            logFF("    %4d: pos=%6llu VITC=%02d:%02d:%02d:%02d LTC=%02d:%02d:%02d:%02d red=%4d fls=%4d spt=%4d ext=%d\n", i, pseFailures[i].position,
                    pseFailures[i].vitcTimecode.hour, pseFailures[i].vitcTimecode.min,
                    pseFailures[i].vitcTimecode.sec, pseFailures[i].vitcTimecode.frame,
                    pseFailures[i].ltcTimecode.hour, pseFailures[i].ltcTimecode.min,
                    pseFailures[i].ltcTimecode.sec, pseFailures[i].ltcTimecode.frame,
                    pseFailures[i].redFlash, pseFailures[i].luminanceFlash, pseFailures[i].spatialPattern, pseFailures[i].extendedFailure);
        }
    }

    // Complete MXF file
    pthread_mutex_lock(&m_mxfout);
    int res = complete_archive_mxf_file(&mxfout, &infaxData, pseFailures, numPSEFailues, vtrErrors, numVTRErrors);
    if (!res) {
        logFF("stop_multi_item_record: Failed to complete writing Archive MXF file\n");
    }
    mxfout = 0;
    pthread_mutex_unlock(&m_mxfout);

    // Get the overall PSE result
    if (pse_enabled)
    {
        bool result = PSEReport::hasPassed(pseFailures, numPSEFailues);
        *pseResult = result ? 0 : 1;
        logFF("get pse result succeeded\n");

        delete [] pseFailures;
    }


    logFF("stop_multi_item_record: finished, g_frames_written=%d\n", g_frames_written);
    return true;
}

bool Capture::abort_record(void)
{
    logFF("abort_record\n");

    if (g_record_start) {
        // suspend the capture thread
        g_suspend_capture = true;
    
        // signal to store thread to stop
        g_record_stop_frame = last_captured();
    
        // wait until writing thread has finished with current frame
        // (an alternative would be to have a mutux around use of mxfout and
        // to stop the writing thread accessing it after abort_archive_mxf_file())
        while (g_record_start) {
            // wake up the store threads
            signal_frame_ready();
            
            sleep_msec(20);
        }
    }
    else {
        // suspend the capture thread
        g_suspend_capture = true;

        // wake up the browse thread
        // eg. the mxf store buffer overflowed and the sdi signal is bad
        signal_frame_ready();
    }


    // abort MXF writing
    pthread_mutex_lock(&m_mxfout);
    bool res = true;
    if (mxfout != 0)
    {
        res = abort_archive_mxf_file(&mxfout);
        logFF("abort_record: abort_archive_mxf_file() returned %d\n", res);
        mxfout = 0;
    
        // delete file
        if (mxf_page_file_is_page_filename(mxf_filename))
        {
            int status = mxf_page_file_remove(mxf_filename);
            logFF("abort_record: mxf_page_file_remove(%s) returned %d\n", mxf_filename, status);
            if (!status) {
                logFF("abort_record: mxf_page_file_remove failed\n");
                res = 0;    // abort failed to remove file
            }
        }
        else
        {
            int status = remove(mxf_filename);
            logFF("abort_record: remove(%s) returned %d\n", mxf_filename, status);
            if (status != 0) {
                logFF("abort_record: remove failed: %s\n", strerror(status));
                res = 0;    // abort failed to remove file
            }
        }
    }
    pthread_mutex_unlock(&m_mxfout);

    // Close PSE analysis
    if (pse_enabled)
    {
        pse->close();
    }

    g_aborted = true;

    logFF("abort_record: returning %d\n", res);
    return res;
}

BrowseFrame* Capture::get_browse_frame_write()
{
    // wait for browse frame to become available to write (!read) 
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    while (browse_frames[(last_browse_frame_write + 1) % 2].readyToRead)
    {
        pthread_cond_wait(&browse_frame_ready_cond, &m_browse_frame_ready_cond);
    }
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
    
    last_browse_frame_write++;
    return &browse_frames[last_browse_frame_write % 2];
}

BrowseFrame* Capture::get_browse_frame_read()
{
    // wait for browse frame to become available to read 
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    while (!browse_frames[(last_browse_frame_read + 1) % 2].readyToRead)
    {
        pthread_cond_wait(&browse_frame_ready_cond, &m_browse_frame_ready_cond);
    }
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
    
    last_browse_frame_read++;
    return &browse_frames[last_browse_frame_read % 2];
}

void Capture::browse_end_sequence()
{
    BrowseFrame* frame = get_browse_frame_write();

    frame->isBlackFrame = false;
    frame->isEndOfSequence = true;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame (end of sequence indicator) available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::browse_add_frame(uint8_t *video, uint8_t *audio, ArchiveTimecode ltc, ArchiveTimecode vitc)
{
    BrowseFrame* frame = get_browse_frame_write();
    
    /* convert video to yuv420 */    
    uyvy_to_yuv420(720, 576, 0, video, frame->video);

    /* The first half of the first line is set to black because it contains PAL decode information, and
    this bursty data will cause disturbances in the surrounding video after encoding. The last half of the
    last line is set to black to be consistent */
    
    /* TODO and WARNING: this assumes the UYVY to YUV420 conversion skips every second line of U and V samples to
    get the vertical downconversion */
    
    /* Note: we don't modify the video buffer because it is used by other threads for writing to file and 
    display */
    
    memset(frame->video, 0x10, 360); /* first line, first half Y */
    memset(frame->video + 720 * 576, 0x80, 180); /* first line, first half U */
    memset(frame->video + 720 * 576 + 360 * 288, 0x80, 180); /* first line, first half V */
    memset(frame->video + 720 * 576 - 360, 0x10, 360); /* last line, second half Y */

    /* convert 2 channel 32 bit dvs audio to 16 bit audio */
    dvsaudio32_to_16bitstereo(audio, frame->audio);
    
    /* timecodes */
    frame->ltc = ltc;
    frame->vitc = vitc;
    
    
    frame->isBlackFrame = false;
    frame->isEndOfSequence = false;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::browse_add_black_frame(ArchiveTimecode ltc, ArchiveTimecode vitc)
{
    BrowseFrame* frame = get_browse_frame_write();
    
    frame->ltc = ltc;
    frame->vitc = vitc;
    
    frame->isBlackFrame = true;
    frame->isEndOfSequence = false;
    
    pthread_mutex_lock(&m_browse_frame_ready_cond);
    frame->readyToRead = true; // frame available for reading
    pthread_cond_signal(&browse_frame_ready_cond);
    pthread_mutex_unlock(&m_browse_frame_ready_cond);
}

void Capture::encode_browse_thread(void)
{
    ThreadRunningGuard guard(&browse_thread_running);
    
    BrowseEncoder* encoder = BrowseEncoder::create(browse_filename, browse_kbps, browse_thread_count);
    if (encoder == 0)
    {
        logTF("Failed to create the browse encoder for file '%s'\n", browse_filename);

        logTF("Browse encode thread exited\n");
        pthread_exit(NULL);
    }
    
    // encode browse frames
    while (true)
    {
        BrowseFrame* frame = get_browse_frame_read();
        if (frame->isEndOfSequence)
        {
            // done encoding
            break;
        }

        if (frame->isBlackFrame)
        {
            if (!encoder->encode(black_browse_video, silent_browse_audio, frame->ltc, frame->vitc, g_browse_written++)) 
            {
                logTF("browse_encoder_encode failed\n");
            }
        }
        else
        {
            if (!encoder->encode(frame->video, frame->audio, frame->ltc, frame->vitc, g_browse_written++)) 
            {
                logTF("browse_encoder_encode failed\n");
            }
        }

        pthread_mutex_lock(&m_browse_frame_ready_cond);
        frame->readyToRead = false; // frame available for writing
        pthread_cond_signal(&browse_frame_ready_cond);
        pthread_mutex_unlock(&m_browse_frame_ready_cond);
    }

    // Complete browse file
    delete encoder;
    
    logTF("Browse encode thread exited\n");
    pthread_exit(NULL);
}


// Read from ring memory buffer, prepare frame for encoding
void Capture::store_browse_thread(void)
{
    int browse_frames_written = 0;
    int overflow_count = 0;

    FILE* timecodeFile = fopen(browse_timecode_filename, "wb");
    if (timecodeFile == 0)
    {
        logTF("Failed to open browse timecode file '%s': %s\n", browse_timecode_filename, strerror(errno));
        
        browse_end_sequence();

        logTF("Browse store thread exited\n");
        pthread_exit(NULL);
    }
    

    // wait for the start signal
    bool noFrames = false;
    while (true)
    {
        // stop recording and exit if no frames recorded
        if (g_record_stop_frame != -2 &&
            g_last_browse_frame_written >= g_record_stop_frame) 
        {
            noFrames = true;
            break;
        }
        
        // g_record_start signals start of write-to-disk
        if (!g_record_start) 
        {
            wait_for_frame_ready();
            continue;
        }
        
        // started
        break;
    }

    if (noFrames)
    {
        browse_end_sequence();
        fclose(timecodeFile);

        logTF("Browse store (preprocess) thread exited\n");
        pthread_exit(NULL);
    }

    // pre-process frames for encode thread and write timecodes
    while (true)
    {
        int last_frame_captured = last_captured();

        // stop recording and exit if reached last frame
        if (g_record_stop_frame != -2 &&
            g_last_browse_frame_written >= g_record_stop_frame) 
        {
            // end of recording
            break;
        }

        // Handle case when nothing available yet (empty buffer indicated by -1)
        if (last_frame_captured == -1) 
        {
            // TODO: abort if capture has signalled start, but no data ready
            wait_for_frame_ready();
            continue;
        }
        
        // If nothing to save, wait until new frame (or frames) ready
        if (g_last_browse_frame_written >= last_frame_captured) 
        {
            wait_for_frame_ready();
            continue;
        }

        // Check we haven't wrapped around ring buffer
        if (overflow_count == 0 && 
            last_frame_captured - g_last_browse_frame_written >= ring_buffer_size) 
        {
            logTF("Browse failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size(%d)\n",
                last_frame_captured, g_last_browse_frame_written, ring_buffer_size);
                
            // we don't stop the recording if the browse fails - just report the problem
            report_browse_buffer_overflow();
            
            // skip 3/4 of the frames to try and catch up
            overflow_count += browse_overflow_frames;
            overflow_count = (overflow_count > ring_buffer_size) ? ring_buffer_size : overflow_count;
            logTF("Writing %d black frames to browse file\n", browse_overflow_frames);

            continue;
        }

        if (overflow_count > 0)
        {
            // write black frame
            
            ArchiveTimecode ltc, vitc, ctc;
            int_to_timecode(0, &ltc);
            int_to_timecode(0, &vitc);
            int_to_timecode(browse_frames_written, &ctc);
            browse_frames_written++;
            
            // Add frame to browse encoding
            browse_add_black_frame(ltc, vitc);
            
            // write timecodes to file
            // control timecode, vertical interval timecode and linear timecode
            write_browse_timecode(timecodeFile, ctc, vitc, ltc);
            
            overflow_count--;
        }
        else
        {
            // next frame which needs to be written
            uint8_t *full_video = rec_ring_frame(g_last_browse_frame_written + 1);
            uint8_t *active_video = full_video + 16 * 720*2; 
        
            // read ltc and vitc from ring buffer
            int ltc_as_int, vitc_as_int;
            memcpy(&ltc_as_int, full_video + ring_ltc_offset(), sizeof(ltc_as_int));
            memcpy(&vitc_as_int, full_video + ring_vitc_offset(), sizeof(vitc_as_int));
    
            ArchiveTimecode ltc, vitc, ctc;
            int_to_timecode(ltc_as_int, &ltc);
            int_to_timecode(vitc_as_int, &vitc);
            int_to_timecode(browse_frames_written, &ctc);
            browse_frames_written++;
            
            // Add frame to browse encoding
            browse_add_frame(active_video, full_video + ring_audio_pair_offset(0), ltc, vitc);
            
            // write timecodes to file
            // control timecode, vertical interval timecode and linear timecode
            write_browse_timecode(timecodeFile, ctc, vitc, ltc);
        }

        g_last_browse_frame_written++;
        
        if (debug_store_thread_buffer)
        {
            logFF("Browse store buffer: position = %d, offset to head = %d\n", 
                g_last_browse_frame_written, last_frame_captured - g_last_browse_frame_written);
        }
    }

    browse_end_sequence();
    fclose(timecodeFile);

    logTF("Browse store thread exited\n");
    pthread_exit(NULL);
}

bool Capture::stop_output_fifo(void)
{
    int res;
    if ((res = sv_fifo_stop(sv, poutput, SV_FIFO_FLAG_FLUSH)) != SV_OK) {
        logTF("sv_fifo_stop(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_stop(poutput)\n");

    if ((res = sv_fifo_free(sv, poutput)) != SV_OK) {
        logTF("sv_fifo_free(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_free(poutput)\n");

    return true;
}

bool Capture::init_start_output_fifo(void)
{
    int res;

    // create FIFO with args bInput=FALSE, bShared(in/out use same mem)=FALSE, bDMA=TRUE
    // fifoflags=0, nframes=0 means use all available card memory
    if ((res = sv_fifo_init(sv, &poutput, FALSE, FALSE, TRUE, 0, 0)) != SV_OK) {
        logTF("sv_fifo_init(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_init(poutput)\n");

    // Start the fifo.  The dropped counter starts counting dropped frames from now.
    if ((res = sv_fifo_start(sv, poutput)) != SV_OK) {
        logTF("sv_fifo_start(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_start(poutput)\n");

    return true;
}

bool Capture::stop_input_fifo(void)
{
    int res;
    if ((res = sv_fifo_stop(sv, pinput, 0)) != SV_OK) {
        logTF("sv_fifo_stop(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_stop(pinput)\n");

    if ((res = sv_fifo_free(sv, pinput)) != SV_OK) {
        logTF("sv_fifo_free(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_free(pinput)\n");

    return true;
}

bool Capture::init_start_input_fifo(void)
{
    int res;

    // create FIFO with args bInput=TRUE, bShared(in/out use same mem)=FALSE, bDMA=TRUE
    // fifoflags=0, nframes=0 means use all available card memory
    if ((res = sv_fifo_init(sv, &pinput, TRUE, FALSE, TRUE, 0, 0)) != SV_OK) {
        logTF("sv_fifo_init(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_init(pinput)\n");

    // Start the fifo.  The dropped counter starts counting dropped frames from now.
    if ((res = sv_fifo_start(sv, pinput)) != SV_OK) {
        logTF("sv_fifo_start(pinput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    logFF("Called sv_fifo_start(pinput)\n");

    return true;
}

void Capture::wait_for_good_SDI_signal(int required_good_frames)
{
    int good_frames = 0;

    // Poll until we get a good video signal for at least required_good_frames
    while (!g_suspend_capture) {
        int videoin, audioin;
        unsigned int h_clock, l_clock;
        int current_tick;
        int res1, res2, res3;
        res1 = sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock);
        res2 = sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin);
        res3 = sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin);
        int query_ok = (res1 == SV_OK && res2 == SV_OK && res3 == SV_OK);

        if (loglev > 2) {
            logFF("wait_for_good_SDI_signal: query_ok=%d tick=%8d video=%s audio=%s\n",
                query_ok,
                current_tick,
                videoin == SV_OK ? "OK" : "error",
                audioin == SV_OK ? "OK" : "error"
                );
        }

        if (query_ok && videoin == SV_OK && audioin == SV_OK) {
            good_frames++;
            if (good_frames == required_good_frames) {
                logFF("wait_for_good_SDI_signal: found %d good frames, returning\n", good_frames);
                break;                          // break from while(!g_suspend_capture) loop
            }
        }

        sleep_msec(18);        // sleep slightly less than one tick (20,000)
    }

    return;
}

void Capture::poll_sdi_signal_status(void)
{
    int videoin, audioin;
    unsigned int h_clock, l_clock;
    int current_tick;
    int res1, res2, res3;
    int query_ok;
    
    res1 = sv_currenttime(sv, SV_CURRENTTIME_CURRENT, &current_tick, &h_clock, &l_clock);
    res2 = sv_query(sv, SV_QUERY_VIDEOINERROR, 0, &videoin);
    res3 = sv_query(sv, SV_QUERY_AUDIOINERROR, 0, &audioin);
    query_ok = (res1 == SV_OK && res2 == SV_OK && res3 == SV_OK);

    set_input_SDI_status(query_ok && videoin == SV_OK, query_ok && audioin == SV_OK);
}

// Capture frames from sv fifo and write to ring buffer
// Read VITC and burnt-in LTC from VBI when using PALFF mode
void Capture::capture_video_thread(void)
{
    // Boolean controlling whether the sv_fifo_getbuffer flushes the fifo. 
    // DVS support suggests this is only done once the first time sv_fifo_getbuffer is called
    bool first_sv_fifo_getbuffer = true;

    // Loop forever capturing frames into ring buffer
    int last_res = SV_OK;
    bool start_input_fifo = true;
    while (1) {
        
        // wait and poll the SDI status if the capture has been suspended
        if (g_suspend_capture)
        {
            logFF("Suspended capturing\n");
            
            if (!start_input_fifo)
            {
                // have started the fifo - stop it 
                stop_input_fifo();
            }
            
            // poll SDI signal status until capture is no longer suspended
            while (g_suspend_capture)
            {
                poll_sdi_signal_status();
                sleep_msec(18);      // sleep slightly less than one tick (20,000)
            }

            start_input_fifo = true;
        }
        
        // start the fifo if neccessary
        if (start_input_fifo)
        {
            init_start_input_fifo();
            start_input_fifo = false;
            first_sv_fifo_getbuffer = true;
            
            logFF("Starting capturing\n");
        }
        else
        {
            first_sv_fifo_getbuffer = false;
        }
        
        
        int last_frame_captured = last_captured();

        // Get sv memmory buffer
        sv_fifo_buffer      *pbuffer;
        int                 res;

        // Tell hardware to capture video (into hardware memory)
        // If sv_fifo_getbuffer() fails it typically takes 20ms for each
        // capture attempt in sv_fifo_getbuffer().
        int flags = first_sv_fifo_getbuffer ? SV_FIFO_FLAG_FLUSH : 0;
        if ((res = sv_fifo_getbuffer(sv, pinput, &pbuffer, NULL, flags)) == SV_OK) {
            
            // sdi signal is good
            set_input_SDI_status(true, true);

            if (res != last_res)
                logFF("capture succeeded with video and audio OK\n");
            last_res = res;
        }
        else {
            logFF("sv_fifo_getbuffer(pinput) failed: %s\n", sv_geterrortext(res));

            // sdi signal is bad
            set_input_SDI_status(false, false);

            // Stop fifo and wait for good signal before continuing
            stop_input_fifo();
            start_input_fifo = true;

            wait_for_good_SDI_signal(32);       // wait for 32 good frames
            
            // Video signal should now be good, try capture again
            // or capture has been suspended
            continue;
        }

        // store new frame into next element in ring buffer
        uint8_t *addr = rec_ring_frame(last_frame_captured + 1);
        pbuffer->dma.addr = (char *)addr;
        pbuffer->dma.size = ring_element_size();
        if (audio_pair_offset[0] == 0) {
            audio_pair_offset[0] = (unsigned long)pbuffer->audio[0].addr[0];
            audio_pair_offset[1] = (unsigned long)pbuffer->audio[0].addr[1];
            audio_pair_offset[2] = (unsigned long)pbuffer->audio[0].addr[2];
            audio_pair_offset[3] = (unsigned long)pbuffer->audio[0].addr[3];
        }

        // Transfer video to host memory
        if ((res = sv_fifo_putbuffer(sv, pinput, pbuffer, NULL)) != SV_OK) {
            logTF("sv_fifo_putbuffer(pinput) failed: %s\n", sv_geterrortext(res));
        }

        if (debug_clapper_avsync) {
            int line_size = 720*2;
            int flash = find_red_flash_uyvy(addr + line_size * 16,  line_size);     // skip 16 lines
            int click1 = 0, click1_off = -1;
            int click2 = 0, click2_off = -1;
            int click3 = 0, click3_off = -1;
            int click4 = 0, click4_off = -1;
            find_audio_click_32bit_stereo(addr + ring_audio_pair_offset(0),
                                                        &click1, &click1_off,
                                                        &click2, &click2_off);
            find_audio_click_32bit_stereo(addr + ring_audio_pair_offset(1),
                                                        &click3, &click3_off,
                                                        &click4, &click4_off);

            if (flash || click1 || click2 || click3 || click4) {
                if (flash)
                    logFF(" %5d  red-flash      \n", last_frame_captured + 1);
                if (click1)
                    logFF(" %5d  a1off=%d %.1fms\n", last_frame_captured + 1, click1_off, click1_off / 1920.0 * 40);
                if (click2)
                    logFF(" %5d  a2off=%d %.1fms\n", last_frame_captured + 1, click2_off, click2_off / 1920.0 * 40);
                if (click3)
                    logFF(" %5d  a3off=%d %.1fms\n", last_frame_captured + 1, click3_off, click3_off / 1920.0 * 40);
                if (click4)
                    logFF(" %5d  a4off=%d %.1fms\n", last_frame_captured + 1, click4_off, click4_off / 1920.0 * 40);
            }
        }

        // get status such as dropped frames
        sv_fifo_info info;
        if (sv_fifo_status(sv, pinput, &info) == SV_OK) {
            hw_dropped_frames = info.dropped;
        }

        if (loglev > 1)
            logFF("last_frame_captured=%6d drop=%d %s",
                last_frame_captured + 1, hw_dropped_frames, g_record_start ? "rec " : "");

        // Do 10bit to 8bit conversion
        //TODO

        // Read two separate timecodes
        int stride = 720*2;

        // VITC
        int i;
        int vitc_as_int = -1;
        for (i = 0; i < num_vitc_lines; i++)
        {
            vitc_as_int = read_timecode(addr, stride, vitc_lines[i], "VITC");
            if (vitc_as_int >= 0)
            {
                // read success
                break;
            }
        }
        memcpy(addr + ring_vitc_offset(), &vitc_as_int, sizeof(vitc_as_int));

        // LTC
        int ltc_as_int = -1;
        for (i = 0; i < num_ltc_lines; i++)
        {
            ltc_as_int = read_timecode(addr, stride, ltc_lines[i], "  LTC");
            if (ltc_as_int >= 0)
            {
                // read success
                break;
            }
        }
        memcpy(addr + ring_ltc_offset(), &ltc_as_int, sizeof(ltc_as_int));

        if (loglev > 1) logFFi("\n");

        // update stats
        current_vitc = vitc_as_int;
        current_ltc = ltc_as_int;

        // update shared variable to signal to disk writing thread
        update_last_captured(last_frame_captured + 1);
    }
}

bool Capture::display_output_caption(void)
{
    int res;
    sv_fifo_buffer *pbuffer;
    if ((res = sv_fifo_getbuffer(sv, poutput, &pbuffer, NULL, 0)) != SV_OK) {
        logTF("display_output_caption: sv_fifo_getbuffer(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    pbuffer->dma.addr = (char *)blank_frame;
    pbuffer->dma.size = video_size;

    if ((res = sv_fifo_putbuffer(sv, poutput, pbuffer, NULL)) != SV_OK) {
        logTF("display_output_caption: sv_fifo_putbuffer(poutput) failed: %s\n", sv_geterrortext(res));
        return false;
    }
    return true;
}

// Read from ring memory buffer, write to disk file
void Capture::store_video_thread(void)
{
    while (1)
    {
        int last_frame_captured = last_captured();
        int last_frame_written = last_written();

        // g_record_start signals start of write-to-disk
        if (! g_record_start) {
            wait_for_frame_ready();
            continue;
        }

        // Indicate failed capture if no good video after grace period
        //set_recording_status(false);

        // stop recording if reached last frame
        // g_record_stop_frame == -1 means stop the record before the first
        // frame was captured
        if (g_record_stop_frame != -2 &&
                last_frame_written >= g_record_stop_frame) {
            // Signal recording stopped
            g_record_start = false;
            g_record_start_frame = -1;
            // Note: we don't reset g_record_stop_frame here becuase the browse thread could
            // still be busy


            wait_for_frame_ready();
            continue;
        }

        // Handle case when nothing available yet (empty buffer indicated by -1)
        if (last_frame_captured == -1) {
            wait_for_frame_ready();
            continue;
        }

        // If nothing to save, wait until new frame (or frames) ready
        if (last_frame_written >= last_frame_captured) {
            wait_for_frame_ready();
            continue;
        }


        // Check we haven't wrapped around ring buffer
        if (last_frame_captured - last_frame_written >= ring_buffer_size) {
            // Indicate to listener that recording failed
            set_recording_status(false);

            logTF("Recording failed: ring buffer overflow: last_frame_captured(%d)-last_frame_written(%d) >= ring_buffer_size(%d)\n",
                last_frame_captured, last_frame_written, ring_buffer_size);

            // Stop recording
            g_record_stop_frame = last_frame_written;

            // report the overflow
            report_mxf_buffer_overflow();
            
            continue;
        }

        // next frame which needs to be written
        uint8_t *full_video = rec_ring_frame(last_frame_written + 1);
        uint8_t *active_video = full_video + 16 * 720*2; 
    
        // read ltc and vitc from ring buffer
        int ltc_as_int, vitc_as_int;
        memcpy(&ltc_as_int, full_video + ring_ltc_offset(), sizeof(ltc_as_int));
        memcpy(&vitc_as_int, full_video + ring_vitc_offset(), sizeof(vitc_as_int));

        ArchiveTimecode ltc, vitc;
        int_to_timecode(ltc_as_int, &ltc);
        int_to_timecode(vitc_as_int, &vitc);

        // Write timecode to MXF file
        CHK(write_timecode(mxfout, vitc, ltc));

        // Record timecodes for later use by PSE report
        timecodes.push_back( make_pair(vitc, ltc) );

        // Write video to MXF file
        CHK(write_video_frame(mxfout, active_video, VIDEO_FRAME_SIZE));

        // Convert and write audio to MXF file
        int i;
        for (i = 0; i < num_audio_tracks; i++)
        {
            dvsaudio32_to_24bitmono(i % 2, full_video + ring_audio_pair_offset(i / 2), audiobuf24bit);
            CHK(write_audio_frame(mxfout, audiobuf24bit, AUDIO_FRAME_SIZE));
        }

        g_frames_written++;

        // preserve first frame recorded to calculate duration later
        if (g_record_start_frame == -1)
            g_record_start_frame = last_frame_written + 1;

        update_last_written(last_frame_written + 1);

        if (debug_clapper_avsync) {
            int line_size = 720*2;
            int flash = find_red_flash_uyvy(active_video, line_size);
            int click1 = 0, click1_off = -1;
            int click2 = 0, click2_off = -1;
            int click3 = 0, click3_off = -1;
            int click4 = 0, click4_off = -1;
            find_audio_click_32bit_stereo(full_video + ring_audio_pair_offset(0),
                                                        &click1, &click1_off,
                                                        &click2, &click2_off);
            find_audio_click_32bit_stereo(full_video + ring_audio_pair_offset(1),
                                                        &click3, &click3_off,
                                                        &click4, &click4_off);

            if (flash || click1 || click2 || click3 || click4) {
                if (flash)
                    logFF(" store %5d  red-flash      \n", last_frame_captured + 1);
                if (click1)
                    logFF(" store %5d  a1off=%d %.1fms\n", last_frame_captured + 1, click1_off, click1_off / 1920.0 * 40);
                if (click2)
                    logFF(" store %5d  a2off=%d %.1fms\n", last_frame_captured + 1, click2_off, click2_off / 1920.0 * 40);
                if (click3)
                    logFF(" store %5d  a3off=%d %.1fms\n", last_frame_captured + 1, click3_off, click3_off / 1920.0 * 40);
                if (click4)
                    logFF(" store %5d  a4off=%d %.1fms\n", last_frame_captured + 1, click4_off, click4_off / 1920.0 * 40);
            }
        }

        // Add frame to PSE analysis
        if (pse_enabled)
        {
            pse->analyse_frame(active_video);
        }


        if (debug_store_thread_buffer)
        {
            logFF("MXF store buffer: position = %d, offset to head = %d\n", 
                last_frame_written + 1, last_frame_captured - (last_frame_written + 1));
        }
    }
}

Capture::Capture(CaptureListener* listener) : _listener(listener)
{
    rec_ring = NULL;
    ring_buffer_size = 125;
    memset(audio_pair_offset, 0, sizeof(audio_pair_offset));

    g_last_frame_captured = -1;
    g_suspend_capture = true;

    g_last_frame_written = -1;
    g_last_browse_frame_written = -1;
    g_record_start_frame = -1;
    g_record_start = false;
    g_record_stop_frame = -2; // -2 means not set, -1 means stop before any frame is captured
    g_frames_written = 0;
    g_browse_written = 0;
    g_materialPackageUID = g_Null_UMID;
    g_filePackageUID = g_Null_UMID;
    g_tapePackageUID = g_Null_UMID;
    g_aborted = false;
    start_vitc = 0;
    start_ltc = 0;
    current_vitc = 0;
    current_ltc = 0;
    last_output_fifo_dropped = 0;
    vitc_lines[0] = 19;
    vitc_lines[1] = 21;
    num_vitc_lines = 2;
    ltc_lines[0] = 15;
    ltc_lines[1] = 17;
    num_ltc_lines = 2;
    num_audio_tracks = 4;
    mxf_page_size = 100 * 1024 * 1024; // 100 MB
    video_ok = false;
    audio_ok = false;
    recording_ok = true;

    strcpy(mxf_filename, "");
    strcpy(browse_filename, "");
    strcpy(browse_timecode_filename, "");
    
    mxfout = 0;

    // contains space for 2 channels at 24bits per sample
    audiobuf24bit = new uint8_t[AUDIO_FRAME_SIZE*2];

    // tmp space for one 720x576x2 YUV420P frame
    video_420 = new uint8_t[720*576*2];
    audio_16bitstereo = new int16_t[1920*2];
    
    // black video and silent audio for browse overflow frames
    silent_browse_audio = new int16_t[1920*2];
    memset(silent_browse_audio, 0, 1920 * 2 * sizeof(int16_t));
    black_browse_video = new uint8_t[720*576*2];
    yuv420p_black_frame(720, 576, black_browse_video);
    

    // Start off with no video or audio, and no recording error
    set_input_SDI_status(false, false);
    set_recording_status(true);

    // Setup captions (buffers need to hold video + audio)
    blank_frame = (uint8_t*)valloc(720*592*2 + 0x8000);
    caption_frame = (uint8_t*)valloc(720*592*2 + 0x8000);
    uyvy_black_frame(720, 592, blank_frame);
    uyvy_color_bars(720, 592, caption_frame);
    memset(blank_frame + 720*592*2, 0, 0x8000);
    memset(caption_frame + 720*592*2, 0, 0x8000);

    
    hw_dropped_frames = 0;

    // Init PSE engine
    pse_enabled = true;
#ifdef HAVE_PSE_FPA
    pse = new PSE_FPA();
#else
    pse = new PSE_Simple();
#endif
    pse->init();

    // Browse
    browse_enabled = true;
    browse_kbps = 2700; // video bit rate
    browse_thread_count = 0; // no threads
    browse_overflow_frames = 50;
    int i;
    for (i = 0; i < 2; i++)
    {
        browse_frames[i].video = new uint8_t[720*576*2];
        browse_frames[i].audio = new int16_t[1920*2];
        browse_frames[i].readyToRead = false;
        browse_frames[i].isEndOfSequence = false;
        browse_frames[i].isBlackFrame = false;
    }
    last_browse_frame_write = 0;
    last_browse_frame_read = 0;
    browse_thread_running = false;

    // debug stuff
    mxf_log = redirect_mxf_logs;        // assign log function
    loglev = 3;
    debug_clapper_avsync = false;
    fp_saved_lines = NULL;
    debug_vitc_failures = false;
    debug_store_thread_buffer = false;
}

Capture::~Capture()
{
    sv_close(sv);

    free(rec_ring);
    delete [] audiobuf24bit;
    delete [] video_420;
    delete [] audio_16bitstereo;
    delete [] black_browse_video;
    delete [] silent_browse_audio;
    delete [] browse_frames[0].video;
    delete [] browse_frames[0].audio;
    delete [] browse_frames[1].video;
    delete [] browse_frames[1].audio;
}

bool Capture::init_capture(int ringBufferSize, VideoMode required_videomode)
{
    ring_buffer_size = ringBufferSize;
    
    logTF("Recorder version: date=%s %s\n", __DATE__, __TIME__);

    // get handle to SDI capture hardware: DVS card 0
    int res;
    char setup[] = "PCI,card=0";
    if ((res = sv_openex(   &sv,
                            setup,
                            SV_OPENPROGRAM_APPLICATION,
                            SV_OPENTYPE_DELAYEDOPEN|SV_OPENTYPE_MASK_MULTIPLE,
                            0,
                            0)) != SV_OK) {
        logTF("card 0: %s\n", sv_geterrortext(res));
        return false;
    }

    // Check if video mode is set to required mode
    if (! check_videomode(sv, required_videomode)) {
        // Only change video mode if necessary (setting mode can take > 1 sec)
        if (! set_videomode(sv, required_videomode)) {
            logTF("Could not set video mode to %s\n", videomode2str(required_videomode));
            return false;
        }
    }
    video_mode = required_videomode;

    // get video dimensions
    sv_info status_info;
    sv_status(sv, &status_info);
    video_width = status_info.xsize;
    video_height = status_info.ysize;
    video_size = video_width * video_height * 2;
    logTF("capture card opened: video raster %dx%d\n", video_width, video_height);

    // set size of one element in ring buffer = video size + audio size
    // each pair of interleaved SDI audio channels consumes 0x4000 bytes
    element_size = video_size + (int)((MAX_CAPTURE_AUDIO + 1) / 2) * 0x4000;

    // allocate ring buffer
    // sv internals need suitably aligned memory, so use valloc not malloc
    rec_ring = (uint8_t*)valloc(element_size * ring_buffer_size);
    if (rec_ring == NULL) {
        logTF("valloc failed\n");
        return false;
    }
    
    // reset the browse buffer overflow if it exceeds ring_buffer_size
    if (browse_overflow_frames > ring_buffer_size)
    {
        browse_overflow_frames = ring_buffer_size;
    }

    // initialise mutexes and condition variables
    pthread_mutex_init(&m_last_frame_captured, NULL);
    pthread_mutex_init(&m_last_frame_written, NULL);
    pthread_mutex_init(&m_frame_ready_cond, NULL);
    pthread_mutex_init(&m_browse_frame_ready_cond, NULL);
    pthread_mutex_init(&m_mxfout, NULL);

    // initialise conditionals to use the monotonic clock
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    if ((res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0)
    {
        logTF("Failed to set monotonic clock for conditional attribute: %s\n", strerror(res));
        return false;
    }
    else
    {
        if ((res = pthread_cond_init(&frame_ready_cond, &attr)) != 0)
        {
            logTF("Failed to initialise conditional: %s\n", strerror(res));
            return false;
        }
    }
    pthread_condattr_destroy(&attr);

    pthread_condattr_init(&attr);
    if ((res = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) != 0)
    {
        logTF("Failed to set monotonic clock for conditional attribute: %s\n", strerror(res));
        return false;
    }
    else
    {
        if ((res = pthread_cond_init(&browse_frame_ready_cond, &attr)) != 0)
        {
            logTF("Failed to initialise conditional: %s\n", strerror(res));
            return false;
        }
    }
    pthread_condattr_destroy(&attr);
    
    
    // Start disk writing thread
    pthread_t   capture_thread;
    if ((res = pthread_create(&capture_thread, NULL, capture_video_thread_wrapper, this)) != 0) {
        logTF("Failed to create write thread: %s\n", strerror(res));
        return false;
    }
    logTF("capture thread id = %lu\n", capture_thread);

    pthread_t   store_thread;
    if ((res = pthread_create(&store_thread, NULL, store_video_thread_wrapper, this)) != 0) {
        logTF("Failed to create store thread: %s\n", strerror(res));
        return false;
    }
    logTF("store thread id = %lu\n", store_thread);

    return true;
}

// debug methods
void Capture::save_video_line(int line_number, uint8_t *line)
{
    if (fp_saved_lines == NULL) {
        if ((fp_saved_lines = fopen("/video/saved_lines.uyvy", "wb")) == NULL) {
            logFF("fopen /video/saved_lines.uyvy failed");
            return;
        }
    }

    if (fwrite(line, 720*2, 1, fp_saved_lines) != 1) {
        logFF("fwrite line %d to saved_lines.uyvy", line_number);
    }
}

