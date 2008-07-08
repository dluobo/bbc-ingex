/*
 * $Id: capture.h,v 1.1 2008/07/08 16:22:31 philipn Exp $
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

#ifndef Capture_h
#define Capture_h

#include <inttypes.h>
#include "dvs_clib.h"
#include "dvs_fifo.h"

#include <write_archive_mxf.h>
#include <mxf/mxf_utils.h>

#include "pse.h"
#include "browse_encoder.h"

#include <string>
#include <vector>
#include <utility>
using std::string;
using std::vector;
using std::pair;

// maximum number of VBI lines that are read for VITC or LTC
#define MAX_VBI_LINES       32

// maximum number of audio streams for capture
#define MAX_CAPTURE_AUDIO   8


enum RecordState { STARTED, IN_PROGRESS, COMPLETED, FAILED };
typedef struct {
    RecordState record_state;
    ArchiveTimecode start_vitc;
    ArchiveTimecode start_ltc;
    ArchiveTimecode current_vitc;
    ArchiveTimecode current_ltc;
    int current_framecount;
    int64_t file_size;
    int64_t browse_file_size;
    mxfUMID materialPackageUID;
    mxfUMID filePackageUID;
    mxfUMID tapePackageUID;
} RecordStats;

typedef struct {
    bool video_ok;
    bool audio_ok;
    bool recording;
} GeneralStats;

typedef enum {
    PAL,            // picture 720x576 at 1/25, using card's h/w vitc reader
    PALFF,          // picture 720x592 at 1/25, using s/w VITC+LTC VBI reader
    NTSC,           // picture 720x486 at 29.97hz, using card's h/w/ vitc
    HD1080i50       // picture 1920x1080 at 25.00hz interlaced, SDI timecode
} VideoMode;

typedef struct
{
    uint8_t* video;
    int16_t* audio;
    ArchiveTimecode ltc;
    ArchiveTimecode vitc;
    bool readyToRead;
    bool isEndOfSequence;
    bool isBlackFrame;
}  BrowseFrame;

class CaptureListener
{
public:
    virtual ~CaptureListener() {}
    
    virtual void sdiStatusChanged(bool videoOk, bool audioOk, bool recordingOk) = 0;
    virtual void mxfBufferOverflow() = 0;
    virtual void browseBufferOverflow() = 0;
};

class Capture
{
public:
    // Constructor is passed an optional Listener which is called
    // whenever a change to the input video/audio status occurs
    Capture(CaptureListener* listener = NULL);

    ~Capture();

    // Create a thread to capture video+audio from hardware to ring buffer.
    // Returns fail if hardware not found or not enough memory.
    bool init_capture(int ringBufferSize, VideoMode videomode = PALFF);

    bool start_record(  const char *filename,
                        const char *browseFilename, 
                        const char *browseTimecodeFilename, 
                        const char *pseReportFilename);
    bool stop_record(   int duration,           // -1 means "stop now"
                        InfaxData* infaxData,
                        VTRError *vtrErrors, int numErrors,
                        int* pseResult); // set to 0 if passed, 1 if failed, -1 if pse analysis failed

    // multi-item start and stop
    bool start_multi_item_record(const char *filename);
    bool stop_multi_item_record(int duration,           // -1 means "stop now"
                                VTRError *vtrErrors, int numErrors,
                                int* pseResult); // set to 0 if passed, 1 if failed, -1 if pse analysis failed

    bool abort_record(void);
    
    void get_general_stats(GeneralStats *p);
    void get_record_stats(RecordStats *p);

    void set_loglev(int level);
    int inc_loglev(void);
    int dec_loglev(void);
    int get_loglev(void);
    
    // only call these functions when not recording
    
    void set_mxf_page_size(int64_t size);
    
    void set_num_audio_tracks(int num);
    
    void set_browse_enable(bool enable);
    void set_browse_video_bitrate(int browsekbps);
    void set_browse_thread_count(int count);
    void set_browse_overflow_frames(int browseOverflowFrames);

    void set_pse_enable(bool enable);
    
    void set_vitc_lines(int* lines, int num_lines);
    void set_ltc_lines(int* lines, int num_lines);
    
    void set_debug_avsync(bool enable);
    void set_debug_vitc_failures(bool enable);
    void set_debug_store_thread_buffer(bool enable);

private:
    friend void *capture_video_thread_wrapper(void *p_obj);
    friend void *store_video_thread_wrapper(void *p_obj);
    friend void *store_browse_thread_wrapper(void *p_obj);
    void capture_video_thread(void);
    void store_video_thread(void);
    void store_browse_thread(void);

    void browse_add_frame(uint8_t *video, uint8_t *audio, ArchiveTimecode ltc, ArchiveTimecode vitc);
    void browse_add_black_frame(ArchiveTimecode ltc, ArchiveTimecode vitc);
    
    friend void *encode_browse_thread_wrapper(void *p_obj);
    void encode_browse_thread(void);
    BrowseFrame* get_browse_frame_write(void);
    BrowseFrame* get_browse_frame_read(void);
    void browse_end_sequence(void);

    int last_captured(void);
    void signal_frame_ready();
    void update_last_captured(int update);
    void wait_for_frame_ready(void);
    int last_written(void);
    void update_last_written(int update);

    uint8_t *rec_ring_frame(int frame);
    int ring_element_size(void);
    int ring_audio_pair_offset(int channelPair);
    int ring_vitc_offset(void);
    int ring_ltc_offset(void);

    bool init_start_input_fifo(void);
    bool init_start_output_fifo(void);
    bool stop_input_fifo(void);
    bool stop_output_fifo(void);
    bool display_output_caption(void);
    void wait_for_good_SDI_signal(int required_good_frames);
    void poll_sdi_signal_status(void);
    void set_recording_status(bool recording_ok);
    void set_input_SDI_status(bool video_ok, bool audio_ok);
    void report_mxf_buffer_overflow();
    void report_browse_buffer_overflow();
    
    int read_timecode(uint8_t* video_data, int stride, int line, const char* type_str);

    pthread_mutex_t m_last_frame_captured;
    pthread_mutex_t m_last_frame_written;
    pthread_mutex_t m_frame_ready_cond;
    pthread_cond_t frame_ready_cond;
    pthread_mutex_t m_mxfout;

    uint8_t *rec_ring;
    int ring_buffer_size;

    int g_last_frame_captured;
    bool g_suspend_capture;

    int g_last_frame_written;
    int g_last_browse_frame_written;
    int g_record_start_frame;
    bool g_record_start;
    int g_record_stop_frame;
    int g_frames_written;
    int g_browse_written;
    bool g_aborted;
    mxfUMID g_materialPackageUID;
    mxfUMID g_filePackageUID;
    mxfUMID g_tapePackageUID;
    int start_vitc, start_ltc, current_vitc, current_ltc;
    int last_output_fifo_dropped;

    char mxf_filename[FILENAME_MAX];
    char browse_filename[FILENAME_MAX];
    char browse_timecode_filename[FILENAME_MAX];
    uint8_t *audiobuf24bit;         // temp buffer for audio conversion
    uint8_t *video_420;             // temp buffer for video 422->420 convert
    int16_t* audio_16bitstereo;     // temp buffer for browse audio conversion
    uint8_t* black_browse_video;
    int16_t* silent_browse_audio;
    ArchiveMXFWriter* mxfout;
    FILE *fp_video;

    sv_handle *sv;
    sv_fifo *pinput;
    sv_fifo *poutput;
    VideoMode video_mode;
    int video_width;
    int video_height;
    int video_size;
    int element_size;
    int audio_pair_offset[4];
    bool video_ok;
    bool audio_ok;
    bool recording_ok;          // current recording (if any) is OK
    CaptureListener* _listener;
    int hw_dropped_frames;
    uint8_t *blank_frame;       // inserted when recording but video input bad
    uint8_t *caption_frame;     // frame to render captions to
    int vitc_lines[MAX_VBI_LINES];
    int num_vitc_lines;
    int ltc_lines[MAX_VBI_LINES];
    int num_ltc_lines;
    int num_audio_tracks;
    int64_t mxf_page_size;

    // PSE analysis
    string pse_report;          // path to pse report file
    PSE_Analyse *pse;           // records all failures to write results at end of capture
    bool pse_enabled;

    // Browse
    bool browse_enabled;
    int browse_kbps;
    int browse_thread_count;
    int browse_overflow_frames;
    BrowseFrame browse_frames[2];
    int last_browse_frame_write;
    int last_browse_frame_read;
    pthread_mutex_t m_browse_frame_ready_cond;
    pthread_cond_t browse_frame_ready_cond;
    bool browse_thread_running;


    // List of all timecodes written to MXF files
    vector< pair<ArchiveTimecode, ArchiveTimecode> >    timecodes;      // VITC, LTC

    // debug stuff
    void save_video_line(int line_number, uint8_t *);
    int loglev;
    int strict_MXF_checking;
    bool debug_clapper_avsync;
    FILE *fp_saved_lines;
    bool debug_vitc_failures;
    bool debug_store_thread_buffer;
};

#endif // Capture_h

