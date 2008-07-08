/*
 * $Id: test_tape.cpp,v 1.1 2008/07/08 16:26:44 philipn Exp $
 *
 * Tests the tape transfer class
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>

#include "logF.h"
#include "tape.h"

#include <write_archive_mxf.h>


#if 0    

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

#endif


int main(int argc, char ** argv)
{
    openLogFileWithDate("test_tape");

    Tape    tape("/dev/nst0");
    tape.init_tape_monitor();

    if (1) {
        const char* infaxString = 
            "D3|"
            "D3 preservation programme|"
            "Phase 3|"
            "2006-02-02|"
            "A|"
            "LME1306H|"
            "71|"
            "T|"
            "2006-01-01|"
            "PROGRAMME BACKING COPY|"
            "Bla bla bla|"
            "1732|"
            "DGN377505|"
            "DC193783|"
            "LONPROG|"
            "1";
        InfaxData infax;
        parse_infax_data(infaxString, &infax, 1);
        
        TapeFileInfo a = {"fileA", "testlocation", infax};
        TapeFileInfo b = {"fileB", "testlocation", infax};
        TapeFileInfo c = {"fileC", "testlocation", infax};
        TapeFileInfoList list;
        list.push_back(a);
        list.push_back(b);
        list.push_back(c);

        tape.store_to_tape(list, "LTO123");

        while (tape.store_pending())
            sleep(1);

        printf("tape no longer store_pending (store_completed=%d)\n", tape.store_completed());

        return 0;
    }

#if 0
    init_terminal();

    attron(A_BOLD);
    mvprintw(0,0,"Uncompressed SDI record to MXF");
    attroff(A_BOLD);

    //mvprintw(1,0,"card 0: %dx%d", status.xsize, status.ysize);

    attron(A_BOLD);
    mvprintw(4,0,"INPUT");
    mvprintw(10,0,"OUTPUT");
    attroff(A_BOLD);

    refresh();

    while (1) {
        int c = getch();
        MT_mvprintw(15,0,"                              ");
        switch (c) {
            case 'q': case 'Q': case 27:    // ESC also quit
                endwin();
                exit(0);
                break;
            case 's': case 'S':
                // start record
                MT_mvprintw(7,0,"disk record: START                    ");
                break;
            case 't': case 'T':
                // stop record
                MT_mvprintw(7,0,"disk record: signalled STOP");
                break;
            default:
                MT_mvprintw(15,0,"Unknown command key (code=%d)", c);
                break;
        }
    }
#endif
}
