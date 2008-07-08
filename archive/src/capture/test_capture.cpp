/*
 * $Id: test_capture.cpp,v 1.1 2008/07/08 16:22:30 philipn Exp $
 *
 * Test recording of a number of frames using the capture.cpp code
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
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
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>

#include "logF.h"
#include "capture.h"

static bool term = false;       // Use ncurses terminal?

static pthread_mutex_t m_screen;
#define MT_mvprintw(...) \
    pthread_mutex_lock(&m_screen); \
    mvprintw(__VA_ARGS__); \
    refresh(); \
    pthread_mutex_unlock(&m_screen);

static void init_terminal(void)
{
    initscr();              // start curses mode
    cbreak();               // turn off line buffering
    keypad(stdscr, TRUE);   // turn on F1, arrow keys etc
    noecho();
    pthread_mutex_init(&m_screen, NULL);
}

typedef struct thread_arg_t {
    Capture     *obj;
} thread_arg_t;

static void *status_thread(void *arg)
{
    Capture *p = ((thread_arg_t*)arg)->obj;

    GeneralStats gen;
    RecordStats rec;

    while (1) {
        p->get_general_stats(&gen);
        if (term) {
            MT_mvprintw(4,0,"INPUT: video %s audio %s", gen.video_ok ? "OK " : "BAD", gen.audio_ok ? "OK " : "BAD");
        }
        else {
            printf("INPUT: video %s audio %s\n", gen.video_ok ? "OK " : "BAD", gen.audio_ok ? "OK " : "BAD");
        }

        if (gen.recording) {
            p->get_record_stats(&rec);
            if (term) {
                MT_mvprintw(5,0,"     : recording: state=%d  framecount=%d  filesize=%llu\n", rec.record_state, rec.current_framecount, rec.file_size);
            }
            else {
                printf("  recording: state=%d framecount=%d filesize=%llu\n", rec.record_state, rec.current_framecount, rec.file_size);
            }
        }
        else {
            if (term)
                MT_mvprintw(5,0,"     : not recording                        ");
        }
        if (term)
            usleep(100 * 1000);     // every 100 ms
        else
            usleep(1000 * 1000);        // every second
    }
}

static void usage_exit(void)
{
    fprintf(stderr, "Usage: test_capture [options] mxf_file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -f <num_frame>    number of frames to signal recording length\n");
    exit(1);
}

int main(int argc, char ** argv)
{
    char *mxf_file = NULL;
    char browse_file[FILENAME_MAX];
    char browse_timecode_file[FILENAME_MAX];
    char pse_file[FILENAME_MAX];
    int num_frames = 75;

    int n;
    for (n = 1; n < argc; n++)
    {
        if (strcmp(argv[n], "-t") == 0) {
            term = true;
        }
        else if (strcmp(argv[n], "-h") == 0) {
            usage_exit();
        }
        else if (strcmp(argv[n], "-f") == 0) {
            if (sscanf(argv[n+1], "%u", &num_frames) != 1)
                usage_exit();
            n++;
        }
        else {
            if (!mxf_file) {
                mxf_file = argv[n];
                continue;
            }
        }
    }

    if (mxf_file == NULL)
        usage_exit();

    // setup file paths
    strcpy(browse_file, mxf_file);
    strcat(browse_file, ".mpg");
    strcpy(browse_timecode_file, mxf_file);
    strcat(browse_timecode_file, ".tc");
    strcpy(pse_file, mxf_file);
    strcat(pse_file, ".html");

    openLogFile("log_test_capture.log");

    Capture capture;
    capture.inc_loglev();
    capture.init_capture(125);
        capture.set_debug_store_thread_buffer(true);

    if (term) {
        init_terminal();

        attron(A_BOLD);
        mvprintw(0,0,"Uncompressed SDI record to MXF");
        attroff(A_BOLD);

        attron(A_BOLD);
        mvprintw(4,0,"INPUT");
        attroff(A_BOLD);

        refresh();
    }

    pthread_t   status_thread_id;
    thread_arg_t status_args = {&capture};
    int err;
    if ((err = pthread_create(&status_thread_id, NULL, status_thread, &status_args)) != 0) {
        fprintf(stderr, "Failed to create status thread: %s\n", strerror(err));
        exit(1);
    }

    const char infax_data_string[] = "D3|D3 preservation programme||2006-02-02||LME1306H|71|T|2006-01-01|PROGRAMME BACKING COPY|Bla bla bla|1732|DGN377505|DC193783|LONPROG|1";
    InfaxData infax_data;
    parse_infax_data(infax_data_string, &infax_data, 1);
    

    if (term) {
        while (1) {
            int psePassed;
            int c = getch();
            MT_mvprintw(15,0,"                              ");
            switch (c) {
                case 'q': case 'Q': case 27:    // ESC also quit
                    endwin();
                    exit(0);
                    break;
                case 's': case 'S':
                    // start record
                    if (capture.start_record(mxf_file, browse_file, browse_timecode_file, pse_file)) {
                        MT_mvprintw(7,0,"start_record: Success                  ");
                    }
                    else {
                        MT_mvprintw(7,0,"start_record: Failed                   ");
                    }
                    break;
                case 't': case 'T':
                    // stop record
                    if (capture.stop_record(-1, &infax_data, NULL, 0, &psePassed)) {
                        MT_mvprintw(7,0,"stop_record: Success       ");
                    }
                    else {
                        MT_mvprintw(7,0,"stop_record: Failed        ");
                    }
                    break;
                default:
                    MT_mvprintw(15,0,"Unknown command key (code=%d)", c);
                    break;
            }
        }
    }

    // non-term mode

    // Wait for good video
    GeneralStats gen;
    do {
        capture.get_general_stats(&gen);
        usleep(500 * 1000);
    } while (! gen.video_ok);

    // Start record
    if (capture.start_record(mxf_file, browse_file, browse_timecode_file, pse_file)) {
        printf("start_record succeeded\n");
    }
    else {
        printf("start_record failed\n");
        exit(1);
    }

    printf("Sleeping for %.2f seconds\n", (num_frames * 40 * 1000) / 1000000.0);
    usleep(num_frames * 40 * 1000);     // sleep for specified number of frames

    int psePassed;
    if (capture.stop_record(-1, &infax_data, NULL, 0, &psePassed)) {
        printf("stop_record succeeded\n");
    }
    else {
        printf("stop_record failed\n");
        exit(1);
    }
}
