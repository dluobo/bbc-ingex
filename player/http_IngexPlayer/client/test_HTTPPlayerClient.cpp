/*
 * $Id: test_HTTPPlayerClient.cpp,v 1.1 2009/02/24 08:21:17 stuart_hc Exp $
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <cstdlib>
#include <memory>
#include <cstring>
#include <signal.h>

#include "HTTPPlayerClient.h"

#include <JogShuttle.h>


using namespace std;
using namespace ingex;


#define CHECK(cmd) \
    if (!(cmd)) \
    { \
        fprintf(stderr, "TEST ERROR: '%s' failed at line %d\n", #cmd, __LINE__); \
        exit(1); \
    }


static const char* g_defaultServerHostName = "localhost";
static const int g_defaultServerPort = 9008;

static HTTPPlayerClient* player = 0;


class TestJogShuttleListener : public JogShuttleListener
{
public:
    TestJogShuttleListener(HTTPPlayerClient* player)
    : _player(player)
    {
    }
    virtual ~TestJogShuttleListener()
    {
    }

    virtual void connected(JogShuttle* jogShuttle, JogShuttleDevice device)
    {
        printf("Jog-shuttle: Connected to '%s'\n", jogShuttle->getDeviceName(device).c_str());
    }

    virtual void disconnected(JogShuttle* jogShuttle, JogShuttleDevice device)
    {
        printf("Jog-shuttle: Disconnected from '%s'\n", jogShuttle->getDeviceName(device).c_str());
    }

    virtual void ping(JogShuttle* jogShuttle)
    {
        // do nothing
    }

    virtual void buttonPressed(JogShuttle* jogShuttle, int number)
    {
        switch (number)
        {
            case 1:
                CHECK(_player->toggleLock());
                break;
            case 2:
                CHECK(_player->nextOSDScreen());
                break;
            case 3:
                CHECK(_player->nextOSDTimecode());
                break;
            case 4:
                CHECK(_player->close());
                exit(0);
                break;
            case 10:
                CHECK(_player->togglePlayPause());
                break;
            case 14:
                CHECK(_player->seek(0, SEEK_SET, FRAME_PLAY_UNIT));
                break;
            case 15:
                CHECK(_player->seek(0, SEEK_END, FRAME_PLAY_UNIT));
                break;
        }

    }

    virtual void buttonReleased(JogShuttle* jogShuttle, int number)
    {
    }

    virtual void jog(JogShuttle* jogShuttle, bool clockwise, int position)
    {
        CHECK(_player->step(clockwise));
    }

    virtual void shuttle(JogShuttle* jogShuttle, bool clockwise, int position)
    {
        int directionFactor = (clockwise ? 1 : -1);
        switch (position)
        {
            case 1:
                CHECK(_player->playSpeed(directionFactor * 1));
                break;
            case 2:
                CHECK(_player->playSpeed(directionFactor * 2));
                break;
            case 3:
                CHECK(_player->playSpeed(directionFactor * 5));
                break;
            case 4:
                CHECK(_player->playSpeed(directionFactor * 10));
                break;
            case 5:
                CHECK(_player->playSpeed(directionFactor * 20));
                break;
            case 6:
                CHECK(_player->playSpeed(directionFactor * 50));
                break;
            case 7:
                CHECK(_player->playSpeed(directionFactor * 100));
                break;
            case 0:
            default:
                CHECK(_player->playSpeed(1));
                break;
        }
    }

private:
    HTTPPlayerClient* _player;
};


class TestIngexPlayerListener : public HTTPPlayerClientListener
{
public:
    TestIngexPlayerListener()
    {
        memset(&_previousState, 0, sizeof(_previousState));
    }

    virtual ~TestIngexPlayerListener() {}

    virtual void stateUpdate(HTTPPlayerClient* client, HTTPPlayerState state)
    {
        if (state.displayedFrame.position >= 0 &&
            state.displayedFrame.position != _previousState.displayedFrame.position)
        {
            // printf("Position = %"PRIi64"\n", state.displayedFrame.position);
        }
        if (state.displayedFrame.position < 0 || // position is negative when starting the player
            state.play != _previousState.play)
        {
            printf("%s\n", (state.play ? "Playing" : "Paused"));
        }

        _previousState = state;
    }

private:
    HTTPPlayerState _previousState;
};


static void catch_sigint(int sig_number)
{
    /* reset signal handlers */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGINT)");
    }
    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
    }

    if (player != 0)
    {
        player->close();
        printf("Closed\n");
    }

    exit(1);
}

static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>> <<inputs>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help               Display this usage message\n");
    fprintf(stderr, "  --server-host <name>     The hostname of the player server (default '%s')\n", g_defaultServerHostName);
    fprintf(stderr, "  --server-port <num>      The player server port (default '%d')\n", g_defaultServerPort);
    fprintf(stderr, "  --dvs-output             Set the server to output to DVS (default is X11)\n");
    fprintf(stderr, "  --dual-output            Set the server to output to both X11 and DVS (default is X11)\n");
    fprintf(stderr, "  --quad-split             Display video in a quad split view (default is no split)\n");
    fprintf(stderr, "Inputs:\n");
    fprintf(stderr, "  --option <name> <value>  Option passed to next input\n");
    fprintf(stderr, "  --mxf <file>             MXF file input\n");
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
    int cmdlnIndex = 1;
    vector<PlayerInput> inputs;
    PlayerInput input;
    string serverName = g_defaultServerHostName;
    int serverPort = g_defaultServerPort;
    bool dvsOutput = false;
    bool dualOutput = false;
    bool quadSplit = false;

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--server-host") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            serverName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--server-port") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &serverPort) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dvs-output") == 0)
        {
            dvsOutput = true;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--dual-output") == 0)
        {
            dualOutput = true;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--quad-split") == 0)
        {
            quadSplit = true;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--option") == 0)
        {
            if (cmdlnIndex + 2 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument(s) for '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            input.options.insert(pair<string, string>(argv[cmdlnIndex + 1], argv[cmdlnIndex + 2]));
            cmdlnIndex += 3;
        }
        else if (strcmp(argv[cmdlnIndex], "--mxf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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
                fprintf(stderr, "Missing argument for '%s'\n", argv[cmdlnIndex]);
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


    // set signal handlers to close on exit
    if (signal(SIGINT, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGINT)");
        return 1;
    }
    if (signal(SIGTERM, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGTERM)");
        return 1;
    }


    JogShuttle* jogShuttle = 0;
    JogShuttleListener* jogShuttleListener = 0;

    player = new HTTPPlayerClient();;
    TestIngexPlayerListener* listener = new TestIngexPlayerListener();
    player->registerListener(listener);

    printf("Version '%s', build '%s'\n", player->getVersion().c_str(), player->getBuildTimestamp().c_str());


    jogShuttle = new JogShuttle();
    jogShuttleListener = new TestJogShuttleListener(player);
    jogShuttle->addListener(jogShuttleListener);

    jogShuttle->start();




    if (!player->connect(serverName, serverPort, 500))
    {
        fprintf(stderr, "Failed to connect to server (http://%s:%d)\n", serverName.c_str(), serverPort);
        return 1;
    }
    printf("Connected to 'http://%s:%d'\n", serverName.c_str(), serverPort);

    if (quadSplit)
    {
        player->setVideoSplit(QUAD_SPLIT_VIDEO_SWITCH);
    }
    if (dualOutput)
    {
        player->setOutputType(DUAL_DVS_AUTO_OUTPUT, 1.0);
        printf("Set output to DUAL\n");
    }
    else if (dvsOutput)
    {
        player->setOutputType(DVS_OUTPUT, 1.0);
        printf("Set output to DVS\n");
    }
    else
    {
        player->setOutputType(X11_AUTO_OUTPUT, 1.0);
        printf("Set output to X11\n");
    }

    CHECK(player->start(inputs, false, 0));
    printf("Started\n");


    while (true)
    {
        sleep(1);
    }


    delete jogShuttle;
    delete jogShuttleListener;

    player->unregisterListener(listener);
    delete listener;
    delete player;

    return 0;
}
