/*
 * $Id: tape.h,v 1.1 2008/07/08 16:26:45 philipn Exp $
 *
 * Transfers a set of MXF files to LTO
 *
 * Copyright (C) 2008 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#ifndef Tape_h
#define Tape_h

#include <inttypes.h>
#include <string>
#include <list>

#include "tapeops.h"

#include <archive_types.h>

using std::string;

// These are maintained to be the same as d3archive::GeneralStatus etc
enum GeneralState { READY, BUSY, NO_TAPE, BAD_TAPE };
enum StoreState { STARTED, IN_PROGRESS, COMPLETED, FAILED };

typedef struct {
    GeneralState tape_state;
    string tape_detail;
    string current_lto_id;
} GeneralStats;

typedef struct {
    string store_filename;
    StoreState store_state;
    int offset;
    string store_detail;
} StoreStats;

typedef struct {
    string  filename;
    string  location;
    InfaxData  infax_data;
} TapeFileInfo;

typedef std::list<TapeFileInfo> TapeFileInfoList;

class Tape
{
public:
    Tape(std::string tapeDevice);
    ~Tape();

    // Create a thread to poll tape drive for tape state
    // and to write to tape when Store() is issued.
    // Returns fail if tape device not found or not enough memory
    bool init_tape_monitor(void);

    bool store_to_tape(TapeFileInfoList list, const char *lto_id);
    bool abort_store(void);

    bool store_completed(void);
    bool store_pending(void);

    void get_general_stats(GeneralStats *p);
    void get_store_stats(StoreStats *p);
    
    void set_log_level(int level);

private:
    friend void *monitor_tape_thread(void *p_obj);
    void monitor_tape(void);

    void set_general_state(GeneralState state);
    void set_store_state(GeneralState state);

    void set_tape_state(TapeDetailedState update, std::string detail);
    TapeDetailedState get_tape_state(void);

    void set_store_pending(bool pending);
    void set_store_completed(bool pending);

    void set_start_store_stats(std::string filename, std::string detail);
    void set_completed_store_stats();
    void set_failed_store_stats(std::string detail);
    
    bool copy_to_tape(void);

    void get_full_video_path(string loc, string file, string *p_outstr);

    void clear_store_stats(bool completed);

    pthread_mutex_t m_current_state;
    pthread_mutex_t m_store_state;
    pthread_mutex_t m_store_pending;

    TapeDetailedState g_current_tape_state;
    bool g_store_pending;
    bool g_store_completed;

    TapeFileInfoList g_store_list;
    string g_lto_id;

    GeneralStats g_general_stats;
    StoreStats  g_store_stats;

    string tape_device;
    
    // debug stuff
    int loglev;
};

#endif // Tape_h
