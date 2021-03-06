/*
 * $Id: test_IngexPlayer.cpp,v 1.12 2009/09/18 16:13:50 philipn Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
 * Modifications: Matthew Marks
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
#include <unistd.h>
#include <time.h>
#include <cstdlib>
#include <memory>
#include <cstring>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>


#include "LocalIngexPlayer.h"
#include "IngexPlayerListener.h"


using namespace prodauto;
using namespace std;


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "TEST ERROR: '%s' failed at line %d\n", #cmd, __LINE__); \
        exit(1); \
    }



class TestIngexPlayerListener : public IngexPlayerListener
{
public:
    TestIngexPlayerListener(LocalIngexPlayer* player, IngexPlayerListenerRegistry* listener_registry)
        : IngexPlayerListener(listener_registry), _player(player), _listener_registry(listener_registry) {};
    virtual ~TestIngexPlayerListener() {};

    virtual void frameDisplayedEvent(const FrameInfo* frameInfo)
    {
        printf("Frame %"PRIi64" displayed\n", frameInfo->position);
    }

    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo)
    {
        printf("Frame %"PRIi64" dropped\n", lastFrameInfo->position);
    }

    virtual void stateChangeEvent(const MediaPlayerStateEvent* event)
    {
        printf("Player state has changed\n");
        if (event->lockedChanged)
        {
            printf("   Locked/unlocked state = %d\n", event->locked);
        }
        if (event->playChanged)
        {
            printf("   play/pause state = %d\n", event->play);
        }
        if (event->stopChanged)
        {
            printf("   stop state = %d\n", event->stop);
        }
        if (event->speedChanged)
        {
            printf("   speed = %dX\n", event->speed);
        }
    }

    virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo)
    {
        printf("End of source reached (%"PRIi64")\n", lastReadFrameInfo->position);
    }

    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo)
    {
        printf("Start of source reached (%"PRIi64")\n", firstReadFrameInfo->position);
    }

    virtual void playerCloseRequested()
    {
        printf("Player close requested - exiting\n");
        exit(1);
    }

    virtual void playerClosed()
    {
        printf("Player has closed\n");
    }

    virtual void keyPressed(int key, int modifier)
    {
        printf("Key pressed %d (modifier=%d)\n", key, modifier);
        if (key == 'q')
        {
            printf("'q' for quit was pressed\n");
            exit(1);
        }
    }

    virtual void keyReleased(int key, int modifier)
    {
        printf("Key released %d (modifier=%d)\n", key, modifier);
    }

    virtual void progressBarPositionSet(float position)
    {
        printf("Progress bar position set at %f\n", position);
        _player->seek((int64_t)(position * 1000), SEEK_SET, PERCENTAGE_PLAY_UNIT);
    }

    virtual void mouseClicked(int imageWidth, int imageHeight, int xPos, int yPos)
    {
        printf("Mouse clicked (x,y)=(%d,%d), (w,h)=(%d,%d)\n", xPos, yPos, imageWidth, imageHeight);
    }


private:
    LocalIngexPlayer* _player;
    IngexPlayerListenerRegistry* _listener_registry;
};


static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>> <<inputs>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help               display this usage message\n");
    fprintf(stderr, "  --window-id <id>         X11 window id to test plugin\n");
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  --option <name> <value>  Option passed to next input\n");
    fprintf(stderr, "  -m | --mxf  <file>       MXF file input\n");
    fprintf(stderr, "  --raw <file>             Raw input\n");
    fprintf(stderr, "  --dv <file>              Raw DV input\n");
    fprintf(stderr, "  --ffmpeg <file>          FFMPEG input\n");
    fprintf(stderr, "  --shm <name>             Shared memory input\n");
    fprintf(stderr, "  --udp <name>             UDP input\n");
    fprintf(stderr, "  --balls                  Balls input\n");
    fprintf(stderr, "  --blank                  Blank input\n");
    fprintf(stderr, "  --clapper                Clapperboard input\n");
    fprintf(stderr, "\n");
}

int main (int argc, const char** argv)
{
    auto_ptr<LocalIngexPlayer> player;
    auto_ptr<IngexPlayerListenerRegistry> listener_registry;
    vector<PlayerInput> inputs;
    vector<bool> opened;
    int cmdlnIndex = 1;
    unsigned long windowId = 0;
    auto_ptr<TestIngexPlayerListener> listener;
    bool dvsCardIsAvailable = false;
    PlayerInput input;

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--window-id") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --window-id\n");
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%lu", &windowId) != 1)
            {
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--option") == 0)
        {
            if (cmdlnIndex + 2 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument(s) for --option\n");
                return 1;
            }
            input.options.insert(pair<string, string>(argv[cmdlnIndex + 1], argv[cmdlnIndex + 2]));
            cmdlnIndex += 3;
        }
        else if (strcmp(argv[cmdlnIndex], "-m") == 0 ||
            strcmp(argv[cmdlnIndex], "--mxf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --mxf\n");
                return 1;
            }
            input.type = MXF_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--raw") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --raw\n");
                return 1;
            }
            input.type = RAW_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dv") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --dv\n");
                return 1;
            }
            input.type = DV_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--ffmpeg") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --ffmpeg\n");
                return 1;
            }
            input.type = FFMPEG_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--shm") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --shm\n");
                return 1;
            }
            input.type = SHM_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--udp") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for --udp\n");
                return 1;
            }
            input.type = UDP_INPUT;
            input.name = argv[cmdlnIndex + 1];
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--balls") == 0)
        {
            input.type = BALLS_INPUT;
            input.name = "";
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--blank") == 0)
        {
            input.type = BLANK_INPUT;
            input.name = "";
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--clapper") == 0)
        {
            input.type = CLAPPER_INPUT;
            input.name = "";
            inputs.push_back(input);
            input.options.clear();
            cmdlnIndex += 1;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    if (inputs.size() == 0)
    {
        usage(argv[0]);
        fprintf(stderr, "No inputs\n");
        return 1;
    }

    input.type = MXF_INPUT;
    input.name = "filenoexist.mxf";
    input.options.clear();
    inputs.push_back(input);
    input.options.clear();



    listener_registry = auto_ptr<IngexPlayerListenerRegistry>(new IngexPlayerListenerRegistry);
    player = auto_ptr<LocalIngexPlayer>(new LocalIngexPlayer(listener_registry.get()));
    player->setOutputType(X11_AUTO_OUTPUT);
    listener = auto_ptr<TestIngexPlayerListener>(new TestIngexPlayerListener(player.get(), listener_registry.get()));

    printf("Version %s, build %s\n", player->getVersion().c_str(),
        player->getBuildTimestamp().c_str());

    // Set plugin window info if any
    X11WindowInfo windowInfo = {NULL, 0, 0, 0};
    if (windowId)
    {
        windowInfo.window = windowId;
        player->setWindowInfo(&windowInfo);
    }

    CHECK(player->setX11WindowName("test 0"));
    CHECK(player->start(inputs, opened, false, 0));
    vector<PlayerInput>::const_iterator iterInputs;
    vector<bool>::const_iterator iterOpened;
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }

    sleep(2);
    printf("REOPEN\n");
    input.options.insert(pair<string, string>("stream_format", "uyvy"));
    input.options.insert(pair<string, string>("width", "100"));
    input.options.insert(pair<string, string>("height", "100"));
    input.type = BALLS_INPUT;
    input.name = "";
    inputs.insert(inputs.begin(), input);
    CHECK(player->start(inputs, opened, true, 0));
    sleep(5);

    inputs.erase(inputs.begin());


    CHECK(player->start(inputs, opened, true, 0));
    printf("Start paused\n");
    sleep(2);

    CHECK(player->start(inputs, opened, true, 25));
    printf("Start paused at frame 25\n");
    sleep(2);

    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);

    CHECK(player->start(inputs, opened, true, 0));
    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);

    CHECK(player->start(inputs, opened, true, 0));
    printf("Actual output type = %d\n", player->getActualOutputType());
    sleep(2);

    player->setVideoSplit(NONA_SPLIT_VIDEO_SWITCH);
    CHECK(player->start(inputs, opened, false, 0));
    sleep(2);

    player->setVideoSplit(QUAD_SPLIT_VIDEO_SWITCH);
    CHECK(player->start(inputs, opened, false, 0));
    sleep(2);

    CHECK(player->close());
    printf("Closed player\n");
    sleep(2);

    dvsCardIsAvailable = player->dvsCardIsAvailable();
    if (dvsCardIsAvailable)
    {
        printf("DVS card is available\n");
        player->setOutputType(DUAL_DVS_AUTO_OUTPUT);
    }
    else
    {
        printf("DVS card is NOT available\n");
    }

    CHECK(player->setX11WindowName("test 1"));
    CHECK(player->start(inputs, opened, false, 0));
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }

    CHECK(player->seek(0, SEEK_END, FRAME_PLAY_UNIT));

    // seeking beyond the eof
    sleep(1);
    printf("Seeking beyond end...\n");
    CHECK(player->seek(-5, SEEK_END, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->setX11WindowName("A new name"));

    sleep(2);
    printf("\nRestarting...\n\n");
    opened.clear();
    CHECK(player->setX11WindowName("test 3"));
    CHECK(player->start(inputs, opened, false, 0));
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }

    sleep(1);
    CHECK(player->step(1));

    sleep(1);
    CHECK(player->step(0));

    sleep(1);
    CHECK(player->togglePlayPause());

    sleep(1);
    CHECK(player->togglePlayPause());

    sleep(1);
    CHECK(player->seek(-5, SEEK_CUR, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->seek(5, SEEK_END, FRAME_PLAY_UNIT));
    CHECK(player->togglePlayPause());

    sleep(1);
    CHECK(player->togglePlayPause());

    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->togglePlayPause());
    CHECK(player->playSpeed(2));

    sleep(1);
    CHECK(player->togglePlayPause());


    sleep(1);
    printf("Audio muted\n");
    CHECK(player->muteAudio(1));

    sleep(3);
    printf("Audio unmuted\n");
    CHECK(player->muteAudio(0));

    sleep(1);
    CHECK(player->mark(0));

    sleep(1);
    CHECK(player->clearMark());

    sleep(1);
    CHECK(player->mark(0));

    sleep(1);
    CHECK(player->clearAllMarks());

    sleep(1);
    CHECK(player->pause());
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    CHECK(player->markPosition(10, 0));
    CHECK(player->markPosition(20, 1));
    CHECK(player->markPosition(30, 2));
    CHECK(player->markPosition(40, 3));

    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());
    sleep(1);
    CHECK(player->seekNextMark());

    sleep(1);
    CHECK(player->clearAllMarks());
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    sleep(1);
    CHECK(player->mark(0));
    sleep(1);
    CHECK(player->seek(50, SEEK_SET, FRAME_PLAY_UNIT));
    sleep(1);
    CHECK(player->mark(0));

    sleep(1);
    CHECK(player->seekPrevMark());

    sleep(1);
    CHECK(player->seekNextMark());

    sleep(1);
    CHECK(player->setOSDScreen(OSD_EMPTY_SCREEN));

    sleep(1);
    CHECK(player->nextOSDScreen());

    sleep(1);
    CHECK(player->nextOSDScreen());

    sleep(1);
    CHECK(player->nextOSDScreen());

    sleep(1);
    CHECK(player->setOSDScreen(OSD_PLAY_STATE_SCREEN));

    sleep(1);
    CHECK(player->nextOSDTimecode());

    sleep(1);
    CHECK(player->nextOSDTimecode());

    sleep(1);
    CHECK(player->switchNextVideo());

    sleep(1);
    CHECK(player->switchPrevVideo());

    sleep(1);
    CHECK(player->switchVideo(1));

    sleep(1);
    CHECK(player->switchVideo(0));

    sleep(1);
    CHECK(player->switchVideo(1));

    sleep(1);
    CHECK(player->switchVideo(2));

    sleep(1);
    CHECK(player->switchVideo(3));

    sleep(1);
    CHECK(player->switchVideo(0));

    sleep(1);
    CHECK(player->switchVideo(2));

    sleep(1);
    CHECK(player->switchNextAudioGroup());

    sleep(1);
    CHECK(player->switchNextAudioGroup());

    sleep(1);
    CHECK(player->switchPrevAudioGroup());

    sleep(1);
    CHECK(player->switchAudioGroup(1));

    sleep(1);
    CHECK(player->switchAudioGroup(2));

    sleep(1);
    CHECK(player->snapAudioToVideo());

    sleep(1);
    CHECK(player->switchVideo(0));

    sleep(1);
    printf("set timecode 0\n");
    CHECK(player->setOSDTimecode(0, -1, -1));

    sleep(1);
    printf("set timecode 1\n");
    CHECK(player->setOSDTimecode(1, -1, -1));

    sleep(1);
    printf("set first control timecode 1\n");
    CHECK(player->setOSDTimecode(0, CONTROL_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set second control timecode 1\n");
    CHECK(player->setOSDTimecode(1, CONTROL_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set first source timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set second source timecode 1\n");
    CHECK(player->setOSDTimecode(1, SOURCE_TIMECODE_TYPE, -1));

    sleep(1);
    printf("set first VITC timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, VITC_SOURCE_TIMECODE_SUBTYPE));

    sleep(1);
    printf("set first LTC timecode 1\n");
    CHECK(player->setOSDTimecode(0, SOURCE_TIMECODE_TYPE, LTC_SOURCE_TIMECODE_SUBTYPE));

    sleep(1);
    CHECK(player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
    CHECK(player->play());


    sleep(2);
    printf("\nRestarting...\n\n");
    opened.clear();
    CHECK(player->setX11WindowName("test 4"));
    CHECK(player->start(inputs, opened, false, 0));
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }
    sleep(4);

    sleep(1);
    CHECK(player->play());

    sleep(1);
    CHECK(player->pause());


    sleep(1);
    printf("\nSwitching output type and restarting...\n\n");
    if (dvsCardIsAvailable)
    {
        player->setOutputType(DUAL_DVS_X11_OUTPUT);
    }
    else
    {
        player->setOutputType(X11_AUTO_OUTPUT);
    }

    opened.clear();
    CHECK(player->setX11WindowName("test 5"));
    CHECK(player->start(inputs, opened, false, 0));
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }
    printf("Actual output type is %d\n", player->getActualOutputType());

    CHECK(player->play());
    sleep(2);

    CHECK(player->pause());
    sleep(1);


    sleep(2);
    printf("\nResetting and restarting (scale == 1.5)...\n\n");
    player->reset();
    sleep(4);

    if (dvsCardIsAvailable)
    {
        player->setOutputType(DUAL_DVS_X11_XV_OUTPUT);
        player->setScale(1.5);
    }
    else
    {
        player->setOutputType(X11_XV_OUTPUT);
        player->setScale(1.5);
    }

    opened.clear();
    CHECK(player->setX11WindowName("test 6"));
    CHECK(player->start(inputs, opened, false, 0));
    for (iterInputs = inputs.begin(), iterOpened = opened.begin();
        iterInputs != inputs.end() && iterOpened != opened.end();
        iterInputs++, iterOpened++)
    {
        if (*iterOpened)
        {
            printf("Opened '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
        else
        {
            printf("Failed to open '%s' (%d)\n", (*iterInputs).name.c_str(), (*iterInputs).type);
        }
    }

    sleep(1);
    CHECK(player->play());

    sleep(1);
    CHECK(player->pause());

    sleep(1);
    CHECK(player->reviewStart(50));

    sleep(1);
    CHECK(player->reviewEnd(50));

    sleep(1);
    CHECK(player->seek(100, SEEK_SET, FRAME_PLAY_UNIT));

    sleep(1);
    CHECK(player->review(50));

    sleep(1);

    // sleep(1);
    // CHECK(player->stop());

    return 0;
}
