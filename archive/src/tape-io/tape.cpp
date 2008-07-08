/*
 * $Id: tape.cpp,v 1.1 2008/07/08 16:26:42 philipn Exp $
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

/*
    An index file is written first to the LTO. The index file contains the list
    of MXF files and their file sizes. The MXF files are renamed using the 
    LTO barcode. The MXF files are also updated with the new filename and 
    LTO Infax data set.
*/
 
// get PRI64d etc. macros
#define __STDC_FORMAT_MACROS 1

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <unistd.h> 
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

#include <write_archive_mxf.h>  // for update_mxf_file()

#include <mxf/mxf_logging.h>    // so we can redirect mxf logs
#include <stdarg.h>             // for va args usage in logs

#include "indexfile.h"
#include "tape.h"
#include "logF.h"


static void sleep_sec(int64_t sec)
{
    struct timespec rem;
    struct timespec tm = {sec, 0};
    
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

static bool file_exists(const string file)
{
    struct stat buf;

    // This is not strictly true, but good enough for our purposes
    // (a component of the path may not have execute permission i.e. EACCES)
    if (stat(file.c_str(), &buf) != 0)
        return false;
    return true;
}

static bool file_readable(const string file)
{
    struct stat buf;
    if (stat(file.c_str(), &buf) != 0)
        return false;

    int mode = buf.st_mode;
    int uid = buf.st_uid;
    int gid = buf.st_gid;
    int euid = geteuid();
    int egid = getegid();

    if (euid == 0)
        return true;
    else if (uid == euid)
        return (mode & S_IRUSR);
    else if (gid == egid)
        return (mode & S_IRGRP);
    else
        return (mode & S_IROTH);
}

static bool file_writable(const string file)
{
    struct stat buf;
    if (stat(file.c_str(), &buf) != 0)
        return false;

    int mode = buf.st_mode;
    int uid = buf.st_uid;
    int gid = buf.st_gid;
    int euid = geteuid();
    int egid = getegid();

    if (euid == 0)
        return true;
    else if (uid == euid)
        return (mode & S_IWUSR);
    else if (gid == egid)
        return (mode & S_IWGRP);
    else
        return (mode & S_IWOTH);
}

// Trap compiles which do not use 64bit filesizes
#if (_FILE_OFFSET_BITS < 64)
#error not using _FILE_OFFSET_BITS >= 64
#endif

static uint64_t get_filesize(const char *file)
{
    struct stat buf;
    if (stat(file, &buf) != 0)
        return 0;

    return buf.st_size;
}

static bool create_link(const char *target, const char *link)
{
    string cmd = "ln -sf ";
    cmd += target;
    cmd += " ";
    cmd += link;
    int res = system(cmd.c_str());
    logTF("create_link(%s, %s) system returned %d\n", target, link, res);
    if (res != 0) {
        return false;
    }
    return true;
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

static const char *state_strings[] = {
    "Failed", "Notape", "Busyload", "Busyloadmom", "Busyeject", "Busywriting", "Writeprotect", "Badtape", "Online" };
static const char* state_str(TapeDetailedState state)
{
    return state_strings[(int)state];
}

static void cleanup_path(string path, string *p_outstr)
{
    bool last_char_is_slash = false; 
    for (unsigned i = 0; i < path.length(); i++) {
        if (last_char_is_slash && path[i] == '/') {
            continue;
        }
        (*p_outstr) += path[i];
        last_char_is_slash = (path[i] == '/');
    }

    return;
}

void Tape::get_full_video_path(string loc, string file, string *p_outstr)
{
    string new_path = loc + "/" + file;
    cleanup_path(new_path, p_outstr);
    return;
}


void Tape::set_log_level(int level)
{
    loglev = level;
}


// Thread safe accessor methods
//

void Tape::get_general_stats(GeneralStats *p)
{
    pthread_mutex_lock(&m_current_state);
    g_general_stats.current_lto_id = g_lto_id;
    *p = g_general_stats;
    pthread_mutex_unlock(&m_current_state);
}

void Tape::get_store_stats(StoreStats *p)
{
    pthread_mutex_lock(&m_store_state);
    *p = g_store_stats;
    pthread_mutex_unlock(&m_store_state);
}

void Tape::set_start_store_stats(string filename, string detail)
{
    pthread_mutex_lock(&m_store_state);
    g_store_stats.store_filename = filename;
    g_store_stats.store_state = IN_PROGRESS;
    g_store_stats.store_detail = detail;
    g_store_stats.offset++;
    pthread_mutex_unlock(&m_store_state);
}

void Tape::set_completed_store_stats()
{
    pthread_mutex_lock(&m_store_state);
    g_store_stats.store_state = COMPLETED;
    pthread_mutex_unlock(&m_store_state);
}

void Tape::set_failed_store_stats(string detail)
{
    pthread_mutex_lock(&m_store_state);
    g_store_stats.store_state = FAILED;
    g_store_stats.store_detail = detail;
    pthread_mutex_unlock(&m_store_state);
}

void Tape::clear_store_stats(bool completed)
{
    pthread_mutex_lock(&m_store_state);
    g_store_stats.store_filename.clear();
    g_store_stats.store_state = completed ? COMPLETED : STARTED;
    g_store_stats.offset = -1;
    g_store_stats.store_detail.clear();
    pthread_mutex_unlock(&m_store_state);
}

void Tape::set_tape_state(TapeDetailedState update, string detail)
{
    pthread_mutex_lock(&m_current_state);

    g_current_tape_state = update;
    g_general_stats.tape_detail = detail;
    switch (update) {
        case Online:
            g_general_stats.tape_state = READY;
            break;
        case Busyload:
            if (detail.empty())
                g_general_stats.tape_detail = "Loading";
            g_general_stats.tape_state = BUSY;
            break;
        case Busyloadmom:
            if (detail.empty())
                g_general_stats.tape_detail = "Loading";
            g_general_stats.tape_state = BUSY;
            break;
        case Busyeject:
            if (detail.empty())
                g_general_stats.tape_detail = "Device in use";
            g_general_stats.tape_state = BUSY;
            break;
        case Busywriting:
            if (detail.empty())
                g_general_stats.tape_detail = "Writing";
            g_general_stats.tape_state = BUSY;
            break;
        case Notape:
            g_general_stats.tape_state = NO_TAPE;
            break;
        case Badtape:
            g_general_stats.tape_state = BAD_TAPE;
            break;
        case Writeprotect:
            if (detail.empty())
                g_general_stats.tape_detail = "Write Protected";
            g_general_stats.tape_state = BAD_TAPE;
            break;
        case Failed:
            g_general_stats.tape_state = BAD_TAPE;
            break;
    }

    pthread_mutex_unlock(&m_current_state);
}

TapeDetailedState Tape::get_tape_state(void)
{
    pthread_mutex_lock(&m_current_state);
    TapeDetailedState state = g_current_tape_state;
    pthread_mutex_unlock(&m_current_state);
    return state;
}

void Tape::set_store_completed(bool completed)
{
    pthread_mutex_lock(&m_store_pending);
    g_store_completed = completed;
    pthread_mutex_unlock(&m_store_pending);
}

bool Tape::store_completed(void)
{
    pthread_mutex_lock(&m_store_pending);
    bool completed = g_store_completed;
    pthread_mutex_unlock(&m_store_pending);
    return completed;
}

void Tape::set_store_pending(bool pending)
{
    pthread_mutex_lock(&m_store_pending);
    g_store_pending = pending;
    pthread_mutex_unlock(&m_store_pending);
}

bool Tape::store_pending(void)
{
    pthread_mutex_lock(&m_store_pending);
    bool pending = g_store_pending;
    pthread_mutex_unlock(&m_store_pending);
    return pending;
}

bool Tape::store_to_tape(TapeFileInfoList list, const char *lto_id)
{
    logTF("store_to_tape(list(size=%d, 1st=%s), lto_id=%s)\n", (int)list.size(), list.front().filename.c_str(), lto_id);

    if (get_tape_state() != Online) {
        logTF("store_to_tape() tape not ready - failed\n");
        return false;
    }

    if (store_pending()) {
        logTF("store_to_tape() store is already pending - failed\n");
        return false;
    }

    // Check each MXF filename exists and is readable
    // and perform update_mxf_file() upon it.
    int file_num = 1;               // start at 1 since we skip index file at 0
    TapeFileInfoList::iterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        string fullpath;
        get_full_video_path((*it).location, (*it).filename, &fullpath);

        // readability
        if (! file_readable(fullpath.c_str())) {
            logTF("store_to_tape() %s does not exist or is not readable - failed\n",
                fullpath.c_str());
            return false;
        }

        // Perform some Infax data checks
        int strict_checking = 0;        // TODO: make this configurable
        if (strcmp((*it).infax_data.format, "LTO") != 0) {
            logTF("store_to_tape() Infax data format is not \"LTO\" (item %d in list, infax format=%s)\n", file_num-1, (*it).infax_data.format);
            if (strict_checking)
                return false;
        }
        // write updated infax data into MXF file
        string newfile;
        lto_rebadge_name(lto_id, file_num++, &newfile);
        if (! update_archive_mxf_file(fullpath.c_str(), newfile.c_str(), &(*it).infax_data)) {
            logTF("update_archive_mxf_file failed (%s, %s)\n", fullpath.c_str(), newfile.c_str());
            return false;
        }
        logTF("  update_archive_mxf_file(%s, %s)\n", fullpath.c_str(), newfile.c_str());
    }

    // Save copy of filelist for tape monitor to use
    g_lto_id = lto_id;
    g_store_list = list;

    // Signal store command has been received and is pending
    logTF("store_to_tape() set store pending\n");
    clear_store_stats(false); // clear previous stats before starting
    set_store_pending(true);
    set_store_completed(false);
    
    return true;
}

bool Tape::abort_store(void)
{
    if (! store_pending()) {
        logTF("abort_store() store is not pending - failed\n");
        return false;
    }

    // If a store is under way, signal for it to stop
    // and in the mean time mark tape as BUSY, and store as FAILED

    logTF("Store aborted\n");
    set_store_pending(false);
    set_store_completed(true);  // also used to send FAILURE

    set_tape_state(Badtape, "Store Aborted");
    g_lto_id.clear();
    set_failed_store_stats("Store Aborted");

    return true;
}

extern void * monitor_tape_thread(void *p_obj)
{
    Tape *p = (Tape *)(p_obj);

    p->monitor_tape();      // never returns
    return NULL;
}

static bool busy(TapeDetailedState state)
{
    if (state == Busyload || state == Busyloadmom || state == Busyeject)
        return true;
    return false;
}

static bool bad(TapeDetailedState state)
{
    if (state == Writeprotect || state == Badtape)
        return true;
    return false;
}
#define CONTINUE_POLL last_state = state; sleep_sec(1); continue

// Monitor tape for tape loaded state and act on Store() command if pending
void Tape::monitor_tape(void)
{
    int busyLoadMomCount = 0;
    
    // Trigger tape read check on program startup (if tape loaded)
    TapeDetailedState last_state = Busyload;

    while (true)
    {
        TapeDetailedState state = probe_tape_status(tape_device.c_str());
        
        // if we remain in the Busyloadmom for more than 5 seconds then we assume the
        // tape was was not positioned at the start and was in the drive 
        // before the server machine was started. We rewind the tape
        if (state == Busyloadmom)
        {
            busyLoadMomCount++;
            if (busyLoadMomCount < 5)
            {
                // < 5 seconds
                CONTINUE_POLL;
            }
            
            // rewind the tape
            set_tape_state(state, "Rewinding");
            rewind_tape(tape_device.c_str());
            busyLoadMomCount = 0;
            CONTINUE_POLL;
        }
        busyLoadMomCount = 0;

        // To avoid Badtape check being lost, do not allow change
        // directly from Bad->Online (must go via busy, notape)
        if (bad(last_state) && state == Online) {
            state = last_state;
            CONTINUE_POLL;
        }

        // If changing from Busy->Online check tape is fresh
        if (busy(last_state) && state == Online) {
            logTF("Checking for fresh tape\n");
            string barcode;
            if (tape_contains_tar_file(tape_device.c_str(), &barcode)) {
                // Badtape: cannot use this tape since a tar file was found
                state = Badtape;

                if (barcode.empty())
                    set_tape_state(Badtape, "Unknown data on tape");
                else {
                    set_tape_state(Badtape, "Tape already used");
                    g_lto_id = barcode;
                }
                logTF("State set to Badtape (barcode=\"%s\")\n", barcode.c_str());
                CONTINUE_POLL;
            }
            logTF("fresh or empty tape found\n");
        }

        // All other transitions permitted
        set_tape_state(state, "");
        if (state != last_state)
            logTF("Changing state from %s to %s\n", state_str(last_state), state_str(state));

        // If no store pending and no change in state, continue to poll
        if (state == last_state && ! store_pending()) {
            CONTINUE_POLL;
        }

        // Clear knowledge of tape if Notape
        if (state == Notape) {
            g_lto_id.clear();
        }

        // Clear tape knowledge if busy (FIXME: testing only)
        if (busy(state))
            g_lto_id.clear();

        // Handle store pending
        if (store_pending()) {
            if (state != Online) {
                CONTINUE_POLL;
            }
            logTF("Store pending and tape Online\n");

            // Do the actual copy to tape
            if (copy_to_tape()) {
                logTF("Store complete\n");
                set_store_pending(false);
                set_store_completed(true);
                continue;       // after good store, get tape status immediately
            }
            else {
                logTF("Store failed\n");
                set_store_pending(false);
                set_store_completed(true);  // also used to send FAILURE
                state = Badtape;
                set_tape_state(Badtape, "Store Failed");
                g_lto_id.clear();
                CONTINUE_POLL;
            }
        }

        // Handle other states
        //

        CONTINUE_POLL;
    }
}

bool Tape::copy_to_tape(void)
{
    clear_store_stats(false);

    set_tape_state(Busywriting, "Rewinding");
    rewind_tape(tape_device.c_str());

    set_tape_params(tape_device.c_str());
    
    set_tape_state(Busywriting, "Writing");
    
    // Create string containing entire index file
    string index_contents = g_lto_id + "\n";
    unsigned index_length = index_contents.size() +
                        g_store_list.size() * 37 +  // 36 chars plus newline
                        boiler_plate.size();

    // Add line representing index file
    char line[FILENAME_MAX];
    unsigned count = 0;
    sprintf(line, "%02d\t%s%02u.txt\t%013u\n", count, g_lto_id.c_str(), count, index_length);
    index_contents += line;
    count++;

    // Add lines for each MXF file
    TapeFileInfoList::iterator it;
    for (it = g_store_list.begin(); it != g_store_list.end(); ++it) {
        string loc = (*it).location;
        string file = (*it).filename;
        string fullpath;
        get_full_video_path(loc, file, &fullpath);

        sprintf(line, "%02d\t%s%02u.mxf\t%013"PRIu64"\n", count, g_lto_id.c_str(), count, get_filesize(fullpath.c_str()));

        index_contents += line;
        count++;
    }
    logTF("Index file before boiler plate:\n%s\n", index_contents.c_str());
    index_contents += boiler_plate;

    // Create index file
    string idx_file = g_lto_id + "00.txt";
    FILE *idx_fp;
    if ((idx_fp = fopen(idx_file.c_str(), "wb")) == NULL ) {
        logTF("Could not create index file %s\n", idx_file.c_str());
        return false;
    }
    if (fwrite(index_contents.c_str(), index_contents.size(), 1, idx_fp) != 1) {
        logTF("Could not write contents of index file\n");
        return false;
    }
    fclose(idx_fp);

    // Store index file
    string cmd = "tar --format=posix -b 512 -c -h -v -f " + tape_device + " " + idx_file;
    set_start_store_stats(idx_file, "index -> " + idx_file);
    int res = system(cmd.c_str());
    logTF("(idx file) system returned %d\n", res);
    if (res != 0) {
        set_tape_state(Badtape, "Bad Media");
        set_failed_store_stats("Bad Media");
        return false;
    }
    // delete index file now that we've used it to copy
    remove(idx_file.c_str());
    set_completed_store_stats();

    // Catch abort signal
    if (!store_pending())
        return false;

    // Store each MXF file to tape
    for (it = g_store_list.begin(); it != g_store_list.end(); ++it) {
        string loc = (*it).location;
        string file = (*it).filename;
        string fullpath;
        get_full_video_path(loc, file, &fullpath);

        // create symbolic link
        char newname[FILENAME_MAX];
        sprintf(newname, "%s%02d.mxf", g_lto_id.c_str(), g_store_stats.offset + 1);
        create_link(fullpath.c_str(), newname);

        // store to tape
        // tar command -h dereferences symbolic links
        string cmd = "tar --format=posix -b 512 -c -h -v -f " + tape_device + " ";
        cmd += newname;
        logTF("%s\n", cmd.c_str());
        set_start_store_stats(file, file + " -> " + newname);

        int res = system(cmd.c_str());
        logTF("(MXF file) system returned %d\n", res);
        if (res != 0) {
            set_tape_state(Badtape, "Bad Media");
            set_failed_store_stats("Bad Media");
            remove(newname);
            return false;
        }

        // delete link now that we've used it to copy
        remove(newname);
        set_completed_store_stats();

        if (!store_pending())       // Catch abort signal
            return false;
    }
    logTF("copy_to_tape: rewinding tape\n");
        set_tape_state(Busywriting, "Rewinding");
    rewind_tape(tape_device.c_str());

    if (!store_pending())       // Catch abort signal
        return false;

    set_tape_state(Busyeject, "");
    eject_tape(tape_device.c_str());

    if (!store_pending())       // Catch abort signal
        return false;

    // Set successfull state
    set_tape_state(Online, "");
    clear_store_stats(true);            // Sets stats store_state to COMPLETED

    logTF("copy_to_tape: returning\n");
    return true;
}

Tape::Tape(string tapeDevice)
{
    // initialise mutexes
    pthread_mutex_init(&m_current_state, NULL);
    pthread_mutex_init(&m_store_state, NULL);
    pthread_mutex_init(&m_store_pending, NULL);

    tape_device = tapeDevice;

    g_store_list.clear();
    g_lto_id.clear();

    set_tape_state(Failed, "");
    set_store_pending(false);
    set_store_completed(false);

    g_general_stats.tape_detail = "";
    g_general_stats.current_lto_id = "";

    clear_store_stats(false);

    // debug stuff
    mxf_log = redirect_mxf_logs;        // assign log function
    loglev = 1;
}

Tape::~Tape()
{
}

bool Tape::init_tape_monitor(void)
{
    // Make sure tape device exists and is both readable and writable
    if (file_readable(tape_device) && file_writable(tape_device)) {
        logTF("Tape device %s exists and is readable and writable\n", tape_device.c_str());
    }
    else {
        if (! file_exists(tape_device))
            logTF("Tape device %s does not exist\n", tape_device.c_str());
        else
            logTF("Tape device %s is not readable and writable\n", tape_device.c_str());
        return false;
    }

    // Start tape monitor thread
    int res;
    pthread_t   monitor_thread;
    if ((res = pthread_create(&monitor_thread, NULL, monitor_tape_thread, this)) != 0) {
        logTF("Failed to create tape monitor thread: %s\n", strerror(res));
        return false;
    }

    return true;
}
