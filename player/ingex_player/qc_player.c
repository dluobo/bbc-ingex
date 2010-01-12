/*
 * $Id: qc_player.c,v 1.12 2010/01/12 16:32:33 john_f Exp $
 *
 *
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "media_player.h"
#include "shuttle_input_connect.h"
#include "keyboard_input_connect.h"
#include "term_keyboard_input.h"
#include "multiple_sources.h"
#include "buffered_media_source.h"
#include "mxf_source.h"
#include "system_timecode_source.h"
#include "x11_xv_display_sink.h"
#include "x11_display_sink.h"
#include "dvs_sink.h"
#include "dual_sink.h"
#include "buffered_media_sink.h"
#include "audio_level_sink.h"
#include "qc_session.h"
#include "qc_lto_access.h"
#include "qc_lto_extract.h"
#include "bouncing_ball_source.h"
#include "qc_http_access.h"
#include "version.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"



#define LOG_CLEAN_PERIOD            30

#define MAX_PB_MARK_SELECTIONS      2


typedef enum
{
    UNKNOWN_OUTPUT = 0,
    DVS_OUTPUT,
    XV_DISPLAY_OUTPUT,
    X11_DISPLAY_OUTPUT,
    DUAL_OUTPUT,
    RAW_STREAM_OUTPUT,
    NULL_OUTPUT,
} OutputType;

typedef struct
{
    MediaSink* mediaSink;
    MediaPlayer* mediaPlayer;
    MediaSource* mediaSource;
    QCSession* qcSession;
    QCLTOAccess* qcLTOAccess;
    QCLTOExtract* qcLTOExtract;
    FILE* bufferStateLogFile;
    QCHTTPAccess* qcHTTPAccess;

    X11DisplaySink* x11DisplaySink;
    X11XVDisplaySink* x11XVDisplaySink;
    DualSink* dualSink;
    DVSSink* dvsSink;

    X11WindowListener x11WindowListener;

    OutputType outputType;

    MarkConfigs markConfigs;

    ShuttleInput* shuttle;
    ShuttleConnect* shuttleConnect;
	pthread_t shuttleThreadId;

    int enableTermKeyboard;
    TermKeyboardInput* termKeyboardInput;
    KeyboardConnect* termKeyboardConnect;
	pthread_t termKeyboardThreadId;
} QCPlayer;

typedef struct
{
    int srcBufferSize;
    OutputType outputType;
    OutputType xOutputType;
    int reviewDuration;
    Rational sourceAspectRatio;
    Rational pixelAspectRatio;
    Rational monitorAspectRatio;
    float scale;
    SDIVITCSource sdiVITCSource;
    int extraSDIVITCSource;
    int disableSDIOSD;
    int disableX11OSD;
    int dvsBufferSize;
    int audioLevelStreams;
    int showFieldSymbol;
    MarkConfigs markConfigs;
    int markPSEFails;
    int markVTRErrors;
    int markDigiBetaDropouts;
    const char* ltoCacheDirectory;
    const char* reportDirectory;
    const char* tapeDevice;
    int enableTermKeyboard;
    const char* deleteScriptName;
    const char* deleteScriptOptions;
    const char* sessionScriptName;
    const char* sessionScriptOptions;
    int writeAllMarks;
    int clipMarkType;
    int pbMarkMask[2];
#if defined(HAVE_SHTTPD)
    int qcHTTPPort;
#endif
    char userName[256];
    char hostName[256];
    int vtrErrorLevel;
} Options;

static const Options g_defaultOptions =
{
    97,
    XV_DISPLAY_OUTPUT,
    XV_DISPLAY_OUTPUT,
    20,
    {0,0},
    {1,1},
    {0,0},
    0.0,
    VITC_AS_SDI_VITC,
    0,
    0,
    0,
    12,
    4,
    0,
    {{{0,{0},0}},0},
    1,
    1,
    1,
    NULL,
    NULL,
    "/dev/nst0",
    0,
    NULL,
    NULL,
    NULL,
    NULL,
    1,
    M0_MARK_TYPE,
    {ALL_MARK_TYPE, 0},
#if defined(HAVE_SHTTPD)
    9006,
#endif
    {0},
    {0},
    VTR_ALMOST_GOOD_LEVEL,
};


static const char* g_logFilePrefix = "qc_player_log_";

static QCPlayer g_player;



static void* shuttle_control_thread(void* arg)
{
    QCPlayer* player = (QCPlayer*)arg;

    shj_start_shuttle(player->shuttle);

    pthread_exit((void *) 0);
}

static void* term_keyboard_control_thread(void* arg)
{
    QCPlayer* player = (QCPlayer*)arg;

    tki_start_term_keyboard(player->termKeyboardInput);

    pthread_exit((void *) 0);
}

static int start_control_threads(QCPlayer* player, Options* options)
{
    /* shuttle input */
    if (!shj_open_shuttle(&g_player.shuttle))
    {
        ml_log_warn("Failed to open shuttle input\n");
        printf("Failed to open shuttle input\n");
    }
    else
    {
        if (!create_joinable_thread(&player->shuttleThreadId, shuttle_control_thread, player))
        {
            ml_log_error("Failed to start shuttle control thread\n");
            shj_close_shuttle(&player->shuttle);
            player->shuttleThreadId = 0;
            return 0;
        }
    }

    /* terminal keyboard input */
    if (options->enableTermKeyboard)
    {
        if (!tki_create_term_keyboard(&g_player.termKeyboardInput))
        {
            ml_log_warn("Failed to create terminal keyboard input\n");
            printf("Failed to create terminal keyboard input\n");
        }
        else
        {
            if (!create_joinable_thread(&player->termKeyboardThreadId, term_keyboard_control_thread, player))
            {
                ml_log_error("Failed to start terminal keyboard control thread\n");
                kip_close(tki_get_keyboard_input(player->termKeyboardInput));
                player->termKeyboardThreadId = 0;
                player->termKeyboardInput = NULL;
                return 0;
            }
        }
    }

    return 1;
}

static void terminate_control_threads(QCPlayer* player)
{
    /* stop, join and free the shuttle */
    if (player->shuttle != NULL)
    {
        if (player->shuttleThreadId != 0)
        {
            join_thread(&player->shuttleThreadId, player->shuttle, shj_stop_shuttle);
        }
        shj_close_shuttle(&player->shuttle);
    }

    /* stop, join and free the terminal keyboard */
    if (player->termKeyboardInput != NULL)
    {
        if (player->termKeyboardThreadId != 0)
        {
            join_thread(&player->termKeyboardThreadId, player->termKeyboardInput, tki_stop_term_keyboard);
        }
        kip_close(tki_get_keyboard_input(player->termKeyboardInput));
        player->termKeyboardInput = NULL;
    }
}

static int connect_to_control_threads(QCPlayer* player, int reviewDuration)
{
    /* shuttle input connect */
    if (player->shuttle != NULL)
    {
        if (!sic_create_shuttle_connect(
            reviewDuration,
            ply_get_media_control(player->mediaPlayer),
            player->shuttle,
            QC_MAPPING,
            &player->shuttleConnect))
        {
            ml_log_error("Failed to connect to shuttle input\n");
            return 0;
        }
    }

    /* terminal keyboard input */
    if (player->termKeyboardInput != NULL)
    {
        if (!kic_create_keyboard_connect(
            reviewDuration,
            ply_get_media_control(player->mediaPlayer),
            tki_get_keyboard_input(player->termKeyboardInput),
            QC_MAPPING,
            &player->termKeyboardConnect))
        {
            ml_log_error("Failed to connect to terminal keyboard input\n");
            return 0;
        }
    }

    return 1;
}

static void disconnect_from_control_threads(QCPlayer* player)
{
    /* shuttle input connect */
    if (player->shuttleConnect != NULL)
    {
        sic_free_shuttle_connect(&player->shuttleConnect);
    }

    /* terminal keyboard input */
    if (player->termKeyboardConnect != NULL)
    {
        kic_free_keyboard_connect(&player->termKeyboardConnect);
    }
}

static void x11_window_close_request(void* data)
{
    mc_stop(ply_get_media_control(g_player.mediaPlayer));
}

static int parse_config_marks(const char* val, MarkConfigs* markConfigs)
{
    const char* configSep = val - 1; /* will + 1 below */
    int i;
    const char* typeStr;
    const char* nameStr;
    const char* endNameStr;
    const char* colourStr;
    int typeBit;
    struct ColourInfo
    {
        const char* name;
        Colour colour;
    } colourInfos[] =
    {
        {"white", WHITE_COLOUR},
        {"light-white", LIGHT_WHITE_COLOUR},
        {"light-grey", LIGHT_GREY_COLOUR},
        {"yellow", YELLOW_COLOUR},
        {"cyan", CYAN_COLOUR},
        {"green", GREEN_COLOUR},
        {"magenta", MAGENTA_COLOUR},
        {"red", RED_COLOUR},
        {"blue", BLUE_COLOUR},
        {"orange", ORANGE_COLOUR}
    };

    if (val == NULL || val[0] == '\0')
    {
        return 0;
    }

    markConfigs->numConfigs = 0;

    do
    {
        /* prepare parse type,name,colour */
        typeStr = configSep + 1;
        nameStr = strchr(typeStr, ',');
        if (nameStr == NULL)
        {
            fprintf(stderr, "Missing mark name\n");
            return 0;
        }
        nameStr += 1;
        while (*nameStr == ' ')
        {
            nameStr++;
        }
        colourStr = strchr(nameStr, ',');
        if (colourStr == NULL)
        {
            fprintf(stderr, "Missing mark colour\n");
            return 0;
        }
        endNameStr = colourStr;
        while (endNameStr != nameStr && *endNameStr == ' ')
        {
            endNameStr--;
        }
        if (nameStr == endNameStr)
        {
            fprintf(stderr, "Empty name string\n");
            return 0;
        }
        colourStr += 1;
        while (*colourStr == ' ')
        {
            colourStr++;
        }

        /* parse type */
        if (sscanf(typeStr, "%d", &typeBit) != 1 || typeBit < 1 || typeBit > 32)
        {
            fprintf(stderr, "Invalid mark type\n");
            return 0;
        }
        markConfigs->configs[markConfigs->numConfigs].type = 1 << (typeBit - 1);

        /* copy name */
        strncpy(markConfigs->configs[markConfigs->numConfigs].name, nameStr,
            ((endNameStr - nameStr) > 32) ? 32 : (endNameStr - nameStr));

        /* parse colour */
        for (i = 0; i < (int)(sizeof(colourInfos) / sizeof(struct ColourInfo)); i++)
        {
            if (strncmp(colourInfos[i].name, colourStr, strlen(colourInfos[i].name)) == 0)
            {
                markConfigs->configs[markConfigs->numConfigs].colour = colourInfos[i].colour;
                break;
            }
        }
        if (i == sizeof(colourInfos) / sizeof(struct ColourInfo))
        {
            fprintf(stderr, "Failed to parse colour\n");
            return 0;
        }

        markConfigs->numConfigs++;
        if (markConfigs->numConfigs > 32)
        {
            fprintf(stderr, "Too many mark configs - only maximum 32 allowed\n");
            return 0;
        }
    }
    while ((configSep = strchr(configSep + 1, ':')) != NULL);

    return 1;
}


static int create_sink(QCPlayer* player, Options* options, const char* x11WindowName)
{
    BufferedMediaSink* bufferedSink = NULL;
    AudioLevelSink* audioLevelSink = NULL;
    int haveNewSink = 0;

    switch (options->outputType)
    {
        case XV_DISPLAY_OUTPUT:
            player->x11WindowListener.data = player;
            player->x11WindowListener.close_request = x11_window_close_request;

            if (player->mediaSink == NULL)
            {
                if (!xvsk_open(options->reviewDuration, options->disableX11OSD, &options->pixelAspectRatio,
                    &options->monitorAspectRatio, options->scale, 1, 0, &player->x11XVDisplaySink))
                {
                    ml_log_error("Failed to open x11 xv display sink\n");
                    fprintf(stderr, "Failed to open x11 xv display sink\n");
                    goto fail;
                }
                player->mediaSink = xvsk_get_media_sink(player->x11XVDisplaySink);
                if (!bms_create(&player->mediaSink, 2, 0, &bufferedSink))
                {
                    ml_log_error("Failed to create bufferred xv display sink\n");
                    fprintf(stderr, "Failed to create bufferred xv display sink\n");
                    goto fail;
                }
                player->mediaSink = bms_get_sink(bufferedSink);

                haveNewSink = 1;
            }

            xvsk_set_window_name(player->x11XVDisplaySink, x11WindowName);
            xvsk_register_window_listener(player->x11XVDisplaySink, &player->x11WindowListener);
            break;

        case X11_DISPLAY_OUTPUT:
            player->x11WindowListener.data = player;
            player->x11WindowListener.close_request = x11_window_close_request;

            if (player->mediaSink == NULL)
            {
                if (!xsk_open(options->reviewDuration, options->disableX11OSD, &options->pixelAspectRatio,
                    &options->monitorAspectRatio, options->scale, 1, 0, &player->x11DisplaySink))
                {
                    ml_log_error("Failed to open x11 display sink\n");
                    fprintf(stderr, "Failed to open x11 display sink\n");
                    goto fail;
                }
                player->mediaSink = xsk_get_media_sink(player->x11DisplaySink);

                if (!bms_create(&player->mediaSink, 2, 0, &bufferedSink))
                {
                    ml_log_error("Failed to create bufferred x11 display sink\n");
                    goto fail;
                }
                player->mediaSink = bms_get_sink(bufferedSink);

                haveNewSink = 1;
            }

            xsk_set_window_name(player->x11DisplaySink, x11WindowName);
            xsk_register_window_listener(player->x11DisplaySink, &player->x11WindowListener);
            break;

        case DVS_OUTPUT:
            if (player->mediaSink == NULL)
            {
                if (!dvs_open(-1, -1, options->sdiVITCSource, options->extraSDIVITCSource, options->dvsBufferSize,
                    options->disableSDIOSD, 0, &player->dvsSink))
                {
                    ml_log_error("Failed to open DVS card sink\n");
                    fprintf(stderr, "Failed to open DVS card sink\n");
                    goto fail;
                }
                player->mediaSink = dvs_get_media_sink(player->dvsSink);

                haveNewSink = 1;
            }
            break;

        case DUAL_OUTPUT:
            player->x11WindowListener.data = player;
            player->x11WindowListener.close_request = x11_window_close_request;

            if (player->mediaSink == NULL)
            {
                if (!dusk_open(options->reviewDuration, -1, -1, options->sdiVITCSource, options->extraSDIVITCSource,
                    options->dvsBufferSize, options->xOutputType == XV_DISPLAY_OUTPUT, options->disableSDIOSD, options->disableX11OSD,
                    &options->pixelAspectRatio, &options->monitorAspectRatio, options->scale, 1, 0, 0, &player->dualSink))
                {
                    ml_log_error("Failed to open dual X11 and DVS sink\n");
                    fprintf(stderr, "Failed to open dual X11 and DVS sink\n");
                    goto fail;
                }
                player->mediaSink = dusk_get_media_sink(player->dualSink);

                haveNewSink = 1;
            }

            dusk_set_x11_window_name(player->dualSink, x11WindowName);
            dusk_register_window_listener(player->dualSink, &player->x11WindowListener);
            break;

        default:
            ml_log_error("Unsupported output type\n");
            goto fail;
    }

    /* create audio level sink */

    if (options->audioLevelStreams > 0 && haveNewSink)
    {
        if (!als_create_audio_level_sink(player->mediaSink, options->audioLevelStreams, -18, &audioLevelSink))
        {
            ml_log_error("Failed to create audio level sink\n");
            goto fail;
        }
        player->mediaSink = als_get_media_sink(audioLevelSink);
    }

    return 1;

fail:
    return 0;
}

static int play_balls(QCPlayer* player, Options* options)
{
    MediaSource* mediaSource = NULL;
    MultipleMediaSources* multipleSource = NULL;
    StreamInfo streamInfo;


    /* create multiple source source */

    if (!mls_create(&options->sourceAspectRatio, -1, &g_palFrameRate, &multipleSource))
    {
        ml_log_error("Failed to create multiple source data\n");
        goto fail;
    }
    player->mediaSource = mls_get_media_source(multipleSource);


    /* create the bouncing balls media source */

    memset(&streamInfo, 0, sizeof(streamInfo));
    streamInfo.type = PICTURE_STREAM_TYPE;
    streamInfo.format = UYVY_FORMAT;
    streamInfo.width = 720;
    streamInfo.height = 576;
    streamInfo.frameRate = g_palFrameRate;
    streamInfo.aspectRatio.num = 4;
    streamInfo.aspectRatio.den = 3;

    if (!bbs_create(&streamInfo, 45000 /* 30 mins */, 5, &mediaSource))
    {
        ml_log_error("Failed to create bouncing balls source\n");
        goto fail;
    }
    if (!mls_assign_source(multipleSource, &mediaSource))
    {
        ml_log_error("Failed to assign media source to multiple source\n");
        goto fail;
    }


    /* add a system timecode source */

    if (!sts_create(get_timecode_now(25), &g_palFrameRate, &mediaSource))
    {
        ml_log_error("Failed to create a system timecode source\n");
        goto fail;
    }
    if (!mls_assign_source(multipleSource, &mediaSource))
    {
        ml_log_error("Failed to assign media source to multiple source\n");
        goto fail;
    }


    /* create or reconnect media sink */

    if (!create_sink(player, options, "BBC QC Player"))
    {
        ml_log_error("Failed to create or reconnect media sink\n");
        goto fail;
    }


    /* create the player */

    if (!ply_create_player(
        player->mediaSource,
        player->mediaSink,
        0,
        0,
        0,
        0,
        1,
        0,
        &g_invalidTimecode,
        &g_invalidTimecode,
        NULL,
        options->pbMarkMask,
        2,
        &player->mediaPlayer))
    {
        ml_log_error("Failed to create media player\n");
        goto fail;
    }

    osd_set_mark_display(msk_get_osd(player->mediaSink), &player->markConfigs);

    ply_enable_clip_marks(player->mediaPlayer, options->clipMarkType);


    /* reconnect the X11 display keyboard and mouse input */

    switch (options->outputType)
    {
        case XV_DISPLAY_OUTPUT:
            xvsk_set_media_control(player->x11XVDisplaySink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        case X11_DISPLAY_OUTPUT:
            xsk_set_media_control(player->x11DisplaySink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        case DUAL_OUTPUT:
            dusk_set_media_control(player->dualSink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        default:
            /* no X11 display */
            break;
    }


    /* reconnect the QC LTO access menu handler */

    if (!qla_connect_to_player(player->qcLTOAccess, player->mediaPlayer))
    {
        ml_log_error("Failed to set QC lTO access menu handler\n");
        goto fail;
    }

    /* reconnect to control threads */

    if (!connect_to_control_threads(player, options->reviewDuration))
    {
        ml_log_error("Failed to set QC lTO access menu handler\n");
        goto fail;
    }


    /* start playing... */

    mc_set_osd_screen(ply_get_media_control(player->mediaPlayer), OSD_MENU_SCREEN);
    if (!ply_start_player(player->mediaPlayer, 0))
    {
        ml_log_error("Media player failed to play\n");
        goto fail;
    }

    return 1;

fail:
    if (mediaSource != NULL)
    {
        msc_close(mediaSource);
    }
    return 0;
}

static int validate_player_quit(MediaPlayer* player, void* data)
{
    int result;
    char sysCmd[256];

    result = ply_qc_quit_validate(player);
    if (result == 0)
    {
        return 1;
    }
    if (result == 1)
    {
        /* allow user to quit when the MXF file is incomplete */
        return 1;
    }

    ml_log_info("QC quit validation returned error code %d\n", result);

    strcpy(sysCmd, "kdialog --error 'Validation Error: ");

    switch (result)
    {
        /* case 1 was dealt with above */
        case 2:
            strcat(sysCmd, "Busy with clip mark");
            break;
        case 3:
            strcat(sysCmd, "Programme start and end not set");
            break;
        default:
            ml_log_error("Unknown QC validation error code %d - software update required\n", result);
            strcat(sysCmd, "Unknown");
            break;
    }
    strcat(sysCmd, "'");

    if (system(sysCmd) != 0)
    {
        ml_log_error("Failed to report QC validation error to user: %s\n", strerror(errno));
    }

    return 0;
}

static int play_archive_mxf_file(QCPlayer* player, int argc, const char** argv, Options* options,
    char* directory, char* name, char* sessionName)
{
    MediaSource* mediaSource = NULL;
    MultipleMediaSources* multipleSource = NULL;
    MXFFileSource* mxfSource = NULL;
    BufferedMediaSource* bufferedSource = NULL;
    char filename[FILENAME_MAX];
    char sessionComments[MAX_SESSION_COMMENTS_SIZE];


    /* create multiple source source */

    if (!mls_create(&options->sourceAspectRatio, -1, &g_palFrameRate, &multipleSource))
    {
        ml_log_error("Failed to create multiple source data\n");
        goto fail;
    }
    player->mediaSource = mls_get_media_source(multipleSource);


    /* open the archive MXF source */

    strcpy(filename, directory);
    strcat_separator(filename);
    strcat(filename, name);

    /* open mxf file */
    if (!mxfs_open(filename, 0, options->markPSEFails, options->markVTRErrors, options->markDigiBetaDropouts, &mxfSource))
    {
        ml_log_error("Failed to open MXF file source '%s'\n", filename);
        goto fail;
    }
    mediaSource = mxfs_get_media_source(mxfSource);
    if (!mls_assign_source(multipleSource, &mediaSource))
    {
        ml_log_error("Failed to assign media source to multiple source\n");
        goto fail;
    }


    /* open buffered media source */

    if (options->srcBufferSize > 0)
    {
        if (!bmsrc_create(player->mediaSource, options->srcBufferSize, 1, -1.0, &bufferedSource))
        {
            ml_log_error("Failed to create buffered media source\n");
            goto fail;
        }
        player->mediaSource = bmsrc_get_source(bufferedSource);
    }


    /* open a new qc session */

    strcpy(filename, directory);
    strcat_separator(filename);
    strcat(filename, name);

    if (!qcs_open(filename, player->mediaSource, argc, argv, name, sessionName,
        options->userName, options->hostName, &player->qcSession))
    {
        fprintf(stderr, "Failed to open QC session with prefix '%s'\n", filename);
        goto fail;
    }

    qla_set_current_play_name(player->qcLTOAccess, directory, name);
    qla_set_current_session_name(player->qcLTOAccess, qcs_get_session_name(player->qcSession));
    qce_set_current_play_name(player->qcLTOExtract, directory, name);


    /* create or reconnect media sink */

    if (!create_sink(player, options, name))
    {
        ml_log_error("Failed to create or reconnect media sink\n");
        goto fail;
    }


    /* create the player */

    if (!ply_create_player(
        player->mediaSource,
        player->mediaSink,
        0,
        0,
        0,
        0,
        0,
        0,
        &g_invalidTimecode,
        &g_invalidTimecode,
        g_player.bufferStateLogFile,
        options->pbMarkMask,
        2,
        &player->mediaPlayer))
    {
        ml_log_error("Failed to create media player\n");
        goto fail;
    }

    osd_set_mark_display(msk_get_osd(player->mediaSink), &player->markConfigs);

    ply_enable_clip_marks(player->mediaPlayer, options->clipMarkType);

    ply_set_qc_quit_validator(player->mediaPlayer, validate_player_quit, player);
    
    mc_set_vtr_error_level(ply_get_media_control(player->mediaPlayer), (VTRErrorLevel)options->vtrErrorLevel);
    ply_register_vtr_error_source(player->mediaPlayer, mxfs_get_vtr_error_source(mxfSource));


    /* restore the marks from the selected session file */

    if (sessionName != NULL && sessionName[0] != '\0')
    {
        strcpy(filename, directory);
        strcat_separator(filename);
        strcat(filename, sessionName);

        if (!qcs_restore_session(ply_get_media_control(g_player.mediaPlayer), filename, sessionComments))
        {
            ml_log_warn("Failed to restore marks from the QC session file '%s'\n", filename);
        }
        else if (player->qcSession != NULL)
        {
            qcs_set_comments(player->qcSession, sessionComments);
        }
    }


    /* remove old session files */

    qla_remove_old_sessions(player->qcLTOAccess, directory, name);



    /* reconnect the X11 display keyboard and mouse input */

    switch (options->outputType)
    {
        case XV_DISPLAY_OUTPUT:
            xvsk_set_media_control(player->x11XVDisplaySink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        case X11_DISPLAY_OUTPUT:
            xsk_set_media_control(player->x11DisplaySink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        case DUAL_OUTPUT:
            dusk_set_media_control(player->dualSink, QC_MAPPING, msk_get_video_switch(player->mediaSink),
                ply_get_media_control(player->mediaPlayer));
            break;
        default:
            /* no X11 display */
            break;
    }


    /* connect qc session to player */

    if (!qcs_connect_to_player(player->qcSession, player->mediaPlayer))
    {
        ml_log_error("Failed to connect the QC session to the player\n");
        goto fail;
    }


    /* reconnect the QC LTO access menu handler */

    if (!qla_connect_to_player(player->qcLTOAccess, player->mediaPlayer))
    {
        ml_log_error("Failed to set QC lTO access menu handler\n");
        goto fail;
    }

    /* reconnect to control threads */

    if (!connect_to_control_threads(player, options->reviewDuration))
    {
        ml_log_error("Failed to set QC lTO access menu handler\n");
        goto fail;
    }

    /* activate user mark set validation */

    ply_activate_qc_mark_validation(player->mediaPlayer);


    /* start playing... */

    ml_log_file_flush();
    qcs_flush(player->qcSession);

    mc_set_osd_screen(ply_get_media_control(player->mediaPlayer), OSD_SOURCE_INFO_SCREEN);
    if (!ply_start_player(player->mediaPlayer, 1))
    {
        ml_log_error("Media player failed to play\n");
        goto fail;
    }

    qcs_flush(player->qcSession);

    return 1;

fail:
    if (mediaSource != NULL)
    {
        msc_close(mediaSource);
    }
    if (player->qcSession != NULL)
    {
        qcs_flush(player->qcSession);
    }
    return 0;
}

static int reset_player(QCPlayer* player, Options* options)
{
    Mark* marks = NULL;
    int numMarks = 0;
    int result;
    int sourceIsComplete;

    sourceIsComplete = msc_is_complete(player->mediaSource);

    msc_close(player->mediaSource);
    player->mediaSource = NULL;

    switch (options->outputType)
    {
        case XV_DISPLAY_OUTPUT:
            xvsk_unset_media_control(player->x11XVDisplaySink);
            xvsk_unregister_window_listener(player->x11XVDisplaySink, &player->x11WindowListener);
            xvsk_set_window_name(player->x11XVDisplaySink, "BBC QC Player");
            break;
        case X11_DISPLAY_OUTPUT:
            xsk_unset_media_control(player->x11DisplaySink);
            xsk_unregister_window_listener(player->x11DisplaySink, &player->x11WindowListener);
            xsk_set_window_name(player->x11DisplaySink, "BBC QC Player");
            break;
        case DUAL_OUTPUT:
            dusk_unset_media_control(player->dualSink);
            dusk_unregister_window_listener(player->dualSink, &player->x11WindowListener);
            dusk_set_x11_window_name(player->dualSink, "BBC QC Player");
            break;
        default:
            /* no X11 display */
            break;
    }

    disconnect_from_control_threads(player);

    if (player->mediaPlayer != NULL)
    {
        /* get marks for writing to qc session below */
        if (player->qcSession != NULL)
        {
            numMarks = ply_get_marks(player->mediaPlayer, &marks);
        }
        ply_close_player(&player->mediaPlayer);
    }

    if (player->qcSession != NULL)
    {
        if (numMarks > 0)
        {
            qcs_write_marks(player->qcSession, options->writeAllMarks, options->clipMarkType,
                &player->markConfigs, marks, numMarks);
            SAFE_FREE(&marks);
            numMarks = 0;
        }

        if (sourceIsComplete)
        {
#if defined(HAVE_SHTTPD)
            qcs_close(&player->qcSession, options->reportDirectory, options->sessionScriptName, options->sessionScriptOptions,
                options->qcHTTPPort);
#else
            qcs_close(&player->qcSession, options->reportDirectory, options->sessionScriptName, options->sessionScriptOptions,
                0);
#endif
        }
        else
        {
            /* source is incomplete and therefore the session script is not called */
            ml_log_warn("Not calling session script because the source is incomplete\n");
            qcs_close(&player->qcSession, NULL, NULL, NULL, 0);
        }
    }

    result = msk_reset_or_close(player->mediaSink);
    if (result != 1 && result != 2)
    {
        ml_log_error("Failed to reset the media sink\n");
        goto fail;
    }
    if (result == 2)
    {
        /* media sink could not be reset and was closed */
        player->mediaSink = NULL;
        player->x11DisplaySink = NULL;
        player->x11XVDisplaySink = NULL;
        player->dualSink = NULL;
        player->dvsSink = NULL;
    }

    if (player->qcLTOAccess != NULL)
    {
        qla_set_current_play_name(player->qcLTOAccess, NULL, NULL);
        qla_set_current_session_name(player->qcLTOAccess, NULL);
    }
    if (player->qcLTOExtract != NULL)
    {
        qce_set_current_play_name(player->qcLTOExtract, NULL, NULL);
    }

    return 1;

fail:
    return 0;
}

static int qc_main(QCPlayer* player, int argc, const char** argv, Options* options)
{
    char directory[FILENAME_MAX];
    char name[FILENAME_MAX];
    char sessionName[FILENAME_MAX];


    while (1)
    {
        /* play qc balls and let user select a file to play or quits */

        if (!play_balls(player, options))
        {
            ml_log_error("Failed to play the balls\n");
            goto fail;
        }
        ml_log_file_flush();


        /* get the filename of the next file to play */

        if (!qla_get_file_to_play(player->qcLTOAccess, directory, name, sessionName))
        {
            /* user quit */
            ml_log_info("User has quit player\n");
            return 1;
        }
        ml_log_info("Selected to play '%s/%s', session '%s'\n", directory, name, sessionName);
        ml_log_file_flush();
        printf("Selected to play '%s/%s', session '%s'\n", directory, name, sessionName);
        fflush(stdout);



        /* play selected files until user quits */

        while (1)
        {
            /* reset */

            if (!reset_player(player, options))
            {
                ml_log_error("Failed to reset the player\n");
                goto fail;
            }
            ml_log_file_flush();

            /* play selected file */

            if (!play_archive_mxf_file(player, argc, argv, options, directory, name, sessionName))
            {
                ml_log_warn("Failed to play '%s/%s'\n", directory, name);
                /* TODO: check if file exists to give more some usefull feedback */
                break;
            }
            ml_log_file_flush();

            /* get the filename of the next file to play */

            if (!qla_get_file_to_play(player->qcLTOAccess, directory, name, sessionName))
            {
                /* user quit */
                ml_log_info("User has quit playing file\n");
                ml_log_file_flush();
                printf("User has quit playing file\n");
                fflush(stdout);
                break;
            }
            ml_log_info("Selected to play '%s/%s', session '%s'\n", directory, name, sessionName);
            ml_log_file_flush();
            printf("Selected to play '%s/%s', session '%s'\n", directory, name, sessionName);
            fflush(stdout);
        }

        /* reset */

        if (!reset_player(player, options))
        {
            ml_log_error("Failed to reset the player\n");
            goto fail;
        }
        ml_log_file_flush();
    }


    ml_log_file_flush();
    return 1;

fail:
    ml_log_file_flush();
    return 0;
}

static void cleanup_exit(int res)
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
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
    }
    if (signal(SIGSEGV, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
    }

    /* close in this order */

    msc_close(g_player.mediaSource);
    g_player.mediaSource = NULL;

    if (g_player.x11XVDisplaySink != NULL)
    {
        xvsk_unset_media_control(g_player.x11XVDisplaySink);
        xvsk_unregister_window_listener(g_player.x11XVDisplaySink, &g_player.x11WindowListener);
    }
    else if (g_player.x11DisplaySink != NULL)
    {
        xsk_unset_media_control(g_player.x11DisplaySink);
        xsk_unregister_window_listener(g_player.x11DisplaySink, &g_player.x11WindowListener);
    }
    else if (g_player.dualSink != NULL)
    {
        dusk_unset_media_control(g_player.dualSink);
        dusk_unregister_window_listener(g_player.dualSink, &g_player.x11WindowListener);
    }

    disconnect_from_control_threads(&g_player);
    terminate_control_threads(&g_player);

    if (g_player.mediaPlayer != NULL)
    {
        ply_close_player(&g_player.mediaPlayer);
    }

    if (g_player.bufferStateLogFile != NULL)
    {
        fclose(g_player.bufferStateLogFile);
        g_player.bufferStateLogFile = NULL;
    }

    msk_close(g_player.mediaSink);
    g_player.mediaSink = NULL;
    g_player.x11DisplaySink = NULL;
    g_player.x11XVDisplaySink = NULL;
    g_player.dualSink = NULL;
    g_player.dvsSink = NULL;

    if (g_player.qcSession != NULL)
    {
        qcs_close(&g_player.qcSession, NULL, NULL, NULL, 0);
    }

    qla_free_qc_lto_access(&g_player.qcLTOAccess);

    /* free after qc lto access */
    qce_free_lto_extract(&g_player.qcLTOExtract);

    qch_free_qc_http_access(&g_player.qcHTTPAccess);

    ml_log_file_close();

    exit(res);
}

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
    if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
    }
    if (signal(SIGSEGV, SIG_IGN) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
    }

    ml_log_info("Received signal %d\n", sig_number);

    /* make sure the terminal settings are restored */
    if (g_player.termKeyboardInput != NULL)
    {
        tki_restore_term_settings(g_player.termKeyboardInput);
    }

    /* make sure the DVS card is closed */
    dvs_close_card(g_player.dvsSink);

    exit(1);
}

static void control_help()
{
    const ControlInputHelp* keyboardHelp = kic_get_qc_control_help();
    const ControlInputHelp* shuttleHelp = sic_get_qc_control_help();
    int i;

    fprintf(stderr, "QC keyboard:\n");
    i = 0;
    while (keyboardHelp[i].input != NULL)
    {
        fprintf(stderr, "  %-15s%s\n", keyboardHelp[i].input, keyboardHelp[i].description);
        i++;
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "QC shuttle:\n");
    i = 0;
    while (shuttleHelp[i].input != NULL)
    {
        fprintf(stderr, "  %-15s%s\n", shuttleHelp[i].input, shuttleHelp[i].description);
        i++;
    }
    fprintf(stderr, "\n");
}

static void usage(const char* cmd)
{
    char nameBuffer[256];
    memset(nameBuffer, 0, sizeof(nameBuffer));

    fprintf(stderr, "Usage: %s [options] [inputs]\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help               Display this usage message\n");
    fprintf(stderr, "  --help-control           Display keyboard and jog-shuttle control help\n");
    fprintf(stderr, "  -v, --version            Display the player version\n");
    fprintf(stderr, "* --tape-cache <dir>       The LTO tape cache directory\n");
    fprintf(stderr, "* --report <dir>           The destination directory for QC and PSE reports\n");
    fprintf(stderr, "  --tape-dev <name>        The tape device filename (default %s)\n", g_defaultOptions.tapeDevice);
    fprintf(stderr, "  --delete-script <name>   The <name> script is called to delete an LTO tape cache directory (delete the directory is the default)\n");
    fprintf(stderr, "  --delete-script-opt <options>   Options string to pass to the delete script before the cache directory name\n");
    fprintf(stderr, "  --session-script <name>   The <name> script is called when a session is completed\n");
    fprintf(stderr, "  --session-script-opt <options>   Options string, in addition to --lto and --session, to pass to the session script\n");
    fprintf(stderr, "  --log-level <level>      Output log level; 0=debug, 1=info, 2=warning, 3=error (default %d)\n", DEBUG_LOG_LEVEL);
    fprintf(stderr, "  --log-remove <days>      Remove log files older than given days (default is %d). 0 means don't remove any.\n", LOG_CLEAN_PERIOD);
    fprintf(stderr, "  --log-buf <name>         Log source and sink buffer state to file\n");
#if defined(HAVE_DVS)
    fprintf(stderr, "  --dvs                    SDI ouput using the DVS card\n");
    fprintf(stderr, "  --dvs-buf <size>         Size of the DVS buffer (default is %d; must be >= %d; 0 means maximim)\n", g_defaultOptions.dvsBufferSize, MIN_NUM_DVS_FIFO_BUFFERS);
    fprintf(stderr, "  --dual                   Dual sink with both DVS card output and X server display output\n");
    fprintf(stderr, "  --disable-sdi-osd        Disable the OSD on the SDI output\n");
    fprintf(stderr, "  --ltc-as-vitc            Outputs the source LTC to the SDI output VITC\n");
    fprintf(stderr, "  --count-as-vitc          Outputs the frame count to the SDI output VITC\n");
    fprintf(stderr, "  --extra-vitc             Outputs extra SDI VITC lines\n");
    fprintf(stderr, "  --extra-ltc_as_vitc      Outputs the source LTC to the extra SDI VITC lines\n");
    fprintf(stderr, "  --extra-count-as-vitc    Outputs the frame count to the extra SDI VITC lines\n");
#endif
    fprintf(stderr, "  --xv                     X11 Xv extension display output (YUV colourspace) (default)\n");
    fprintf(stderr, "  --x11                    X11 display output (RGB colourspace)\n");
    fprintf(stderr, "  --disable-x11-osd        Disable the OSD on the X11 or X11 Xv output\n");
    fprintf(stderr, "  --src-buf  <size>        Size of the media source buffer (default is %d)\n", g_defaultOptions.srcBufferSize);
    fprintf(stderr, "  --no-pse-fails           Don't add marks for PSE failures recorded in the archive MXF file\n");
    fprintf(stderr, "  --no-vtr-errors          Don't add marks for VTR playback errors recorded in the archive MXF file\n");
    fprintf(stderr, "  --no-digi-dropouts       Don't add marks for DigiBeta dropouts recorded in the archive MXF file\n");
    fprintf(stderr, "  --vtr-error-level <val>  Set the initial minimum VTR error level. 0 means no errors. Max value is %d (default 1)\n", VTR_NO_GOOD_LEVEL);
    fprintf(stderr, "  --pixel-aspect <W:H>     Video pixel aspect ratio of the display (default 1:1)\n");
    fprintf(stderr, "  --monitor-aspect <W:H>   Pixel aspect ratio is calculated using the screen resolution and this monitor aspect ratio\n");
    fprintf(stderr, "  --source-aspect <W:H>    Force the video aspect ratio (currently only works for the X11 Xv extension output)\n");
    fprintf(stderr, "  --scale <float>          Scale the video (currently only works for the X11 Xv extension output)\n");
    fprintf(stderr, "  --review-dur <sec>       Review duration in seconds (default %d seconds)\n", g_defaultOptions.reviewDuration);
    fprintf(stderr, "  --audio-mon <num>        Display audio levels for first <num> audio streams (default %d, maximum 16)\n", g_defaultOptions.audioLevelStreams);
    fprintf(stderr, "  --show-field-symbol      Show the field symbol for the even fields in the OSD\n");
    fprintf(stderr, "  --config-marks <val>     Configure how marks are named and displayed\n");
    fprintf(stderr, "                               val = \"type,name,colour(:type,name,colour)*\"), where\n");
    fprintf(stderr, "                                  type is 1...32 (bit position in the 32-bit value used internally),\n");
    fprintf(stderr, "                                  name is a string with maximum length 31 and\n");
    fprintf(stderr, "                                  colour is one of white|yellow|cyan|green|magenta|red|blue|orange\n");
    fprintf(stderr, "  --enable-term-keyb       Enable terminal window keyboard input.\n");
    fprintf(stderr, "  --clip-mark <type>       Clip mark type (default 0x%04x).\n", g_defaultOptions.clipMarkType);
    fprintf(stderr, "  [--pb-mark-mask <val>]*  32-bit mask for marks to show on the (next) progress bar (decimal or 0x hex)\n");
#if defined(HAVE_SHTTPD)
    fprintf(stderr, "  --qc-http-access <port>  QC Report access via http <port> (default %d)\n", g_defaultOptions.qcHTTPPort);
#endif
    fprintf(stderr, "  --user <name>            User (operator) name set in the QC session (default '%s')\n", get_user_name(nameBuffer, sizeof(nameBuffer)));
    fprintf(stderr, "  --host <name>            Hostname set in the QC session (default '%s')\n", get_host_name(nameBuffer, sizeof(nameBuffer)));
    fprintf(stderr, "\n");
}

int main(int argc, const char **argv)
{
    int cmdlnIndex = 1;
    int logLevel = DEBUG_LOG_LEVEL;
    Options options;
    struct stat statBuf;
    char logFilename[FILENAME_MAX];
    char timestampStr[MAX_TIMESTAMP_STRING_SIZE];
    DIR* cacheDirStream = NULL;
    struct dirent* cacheDirent;
    time_t now;
    char rmCmd[FILENAME_MAX];
    int logRemoveDays = 30;
    const char* bufferStateLogFilename = NULL;
    int i;
    int numMarkSelections = 0;

    memset(&options, 0, sizeof(options));
    options = g_defaultOptions;
    get_user_name(options.userName, sizeof(options.userName));
    get_host_name(options.hostName, sizeof(options.hostName));

    memset(&g_player, 0, sizeof(g_player));

    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--help-control") == 0)
        {
            control_help();
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
            strcmp(argv[cmdlnIndex], "--version") == 0)
        {
            printf("Version: %s, build: %s\n", get_player_version(), get_player_build_timestamp());
            cmdlnIndex += 1;
            if (cmdlnIndex == argc)
            {
                /* exit if this was the only parameter */
                return 0;
            }
        }
        else if (strcmp(argv[cmdlnIndex], "--log-level") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &logLevel) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--log-remove") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &logRemoveDays) != 1 || logRemoveDays < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--log-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            bufferStateLogFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
#if defined(HAVE_DVS)
        else if (strcmp(argv[cmdlnIndex], "--dvs") == 0)
        {
            options.outputType = DVS_OUTPUT;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--dvs-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &options.dvsBufferSize) != 1 ||
                (options.dvsBufferSize != 0 && options.dvsBufferSize < MIN_NUM_DVS_FIFO_BUFFERS))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dual") == 0)
        {
            options.outputType = DUAL_OUTPUT;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-sdi-osd") == 0)
        {
            options.disableSDIOSD = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--ltc-as-vitc") == 0)
        {
            options.sdiVITCSource = LTC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--count-as-vitc") == 0)
        {
            options.sdiVITCSource = COUNT_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-vitc") == 0)
        {
            options.extraSDIVITCSource = VITC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-ltc-as-vitc") == 0)
        {
            options.extraSDIVITCSource = LTC_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--extra-count-as-vitc") == 0)
        {
            options.extraSDIVITCSource = COUNT_AS_SDI_VITC;
            cmdlnIndex += 1;
        }
#endif
        else if (strcmp(argv[cmdlnIndex], "--xv") == 0)
        {
            options.xOutputType = XV_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)
            if (options.outputType != DUAL_OUTPUT)
            {
#endif
                options.outputType = XV_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)
            }
#endif
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--x11") == 0)
        {
            options.xOutputType = X11_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)
            if (options.outputType != DUAL_OUTPUT)
            {
#endif
                options.outputType = X11_DISPLAY_OUTPUT;
#if defined(HAVE_DVS)
            }
#endif
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--disable-x11-osd") == 0)
        {
            options.disableX11OSD = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--src-buf") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &options.srcBufferSize) != 1 || options.srcBufferSize < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--no-pse-fails") == 0)
        {
            options.markPSEFails = 0;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--no-vtr-errors") == 0)
        {
            options.markVTRErrors = 0;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--no-digi-dropouts") == 0)
        {
            options.markDigiBetaDropouts = 0;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--vtr-error-level") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &options.vtrErrorLevel) != 1 ||
                options.vtrErrorLevel < 0 || options.vtrErrorLevel > VTR_NO_GOOD_LEVEL)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--pixel-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &options.pixelAspectRatio.num, &options.pixelAspectRatio.den) != 2 ||
                options.pixelAspectRatio.num <= 0 || options.pixelAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--monitor-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &options.monitorAspectRatio.num, &options.monitorAspectRatio.den) != 2 ||
                options.monitorAspectRatio.num <= 0 || options.monitorAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--source-aspect") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%u:%u", &options.sourceAspectRatio.num, &options.sourceAspectRatio.den) != 2 ||
                options.sourceAspectRatio.num <= 0 || options.sourceAspectRatio.den <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--scale") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%f", &options.scale) != 1 || options.scale <= 0.0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--review-dur") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &options.reviewDuration) != 1 || options.reviewDuration <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--audio-mon") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &options.audioLevelStreams) != 1 ||
                options.audioLevelStreams < 0 || options.audioLevelStreams > 16)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--show-field-symbol") == 0)
        {
            options.showFieldSymbol = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--config-marks") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (!parse_config_marks(argv[cmdlnIndex + 1], &options.markConfigs))
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--tape-cache") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.ltoCacheDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--report") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.reportDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--tape-dev") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.tapeDevice = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--delete-script") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.deleteScriptName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--delete-script-opt") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.deleteScriptOptions = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--session-script") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.sessionScriptName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--session-script-opt") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            options.sessionScriptOptions = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--enable-term-keyb") == 0)
        {
            options.enableTermKeyboard = 1;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--clip-mark") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &options.clipMarkType) != 1 ||
                options.clipMarkType < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--pb-mark-mask") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (numMarkSelections >= MAX_PB_MARK_SELECTIONS)
            {
                fprintf(stderr, "Only %d mark selection masks supported\n", MAX_PB_MARK_SELECTIONS);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "0x%x\n", &options.pbMarkMask[numMarkSelections]) != 1 &&
                sscanf(argv[cmdlnIndex + 1], "%d\n", &options.pbMarkMask[numMarkSelections]) != 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            numMarkSelections++;
            cmdlnIndex += 2;
        }
#if defined(HAVE_SHTTPD)
        else if (strcmp(argv[cmdlnIndex], "--qc-http-access") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &options.qcHTTPPort) != 1 || options.qcHTTPPort < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
#endif
        else if (strcmp(argv[cmdlnIndex], "--user") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            strncpy(options.userName, argv[cmdlnIndex + 1], sizeof(options.userName) - 1);
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--host") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            strncpy(options.hostName, argv[cmdlnIndex + 1], sizeof(options.hostName) - 1);
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    if (options.ltoCacheDirectory == NULL)
    {
        usage(argv[0]);
        fprintf(stderr, "Missing cache directory option '--tape-cache'\n");
        return 1;
    }
    if (options.reportDirectory == NULL)
    {
        usage(argv[0]);
        fprintf(stderr, "Missing QC and PSE report directory option '--report'\n");
        return 1;
    }


    /* set default mark configs if neccessary */

    if (options.markConfigs.numConfigs == 0)
    {
        MarkConfig configs[] =
        {
            {M0_MARK_TYPE, "red", RED_COLOUR},
            {M1_MARK_TYPE, "magenta", MAGENTA_COLOUR},
            {M2_MARK_TYPE, "green", GREEN_COLOUR},
            {M3_MARK_TYPE, "blue", BLUE_COLOUR},
            {M4_MARK_TYPE, "cyan", CYAN_COLOUR},
            {VTR_ERROR_MARK_TYPE, "yellow (VTR)", YELLOW_COLOUR},
            {PSE_FAILURE_MARK_TYPE, "orange (PSE)", ORANGE_COLOUR},
            {DIGIBETA_DROPOUT_MARK_TYPE, "light-grey (DigiB dropout)", LIGHT_GREY_COLOUR},
        };
        memcpy(&g_player.markConfigs.configs, configs, sizeof(configs));
        g_player.markConfigs.numConfigs = sizeof(configs) / sizeof(MarkConfig);
    }


    /* set signal handlers to clean up cleanly */
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
    if (signal(SIGHUP, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGHUP)");
        return 1;
    }
    if (signal(SIGSEGV, catch_sigint) == SIG_ERR)
    {
        perror("signal(SIGSEGV)");
        return 1;
    }


    /* check the LTO cache directory exists */

    if (stat(options.ltoCacheDirectory, &statBuf) != 0)
    {
        perror("stat");
        fprintf(stderr, "Failed to detect the LTO cache directory\n");
        return 1;
    }
    if (!S_ISDIR(statBuf.st_mode))
    {
        fprintf(stderr, "The LTO cache name, '%s', is not a directory\n", options.ltoCacheDirectory);
        return 1;
    }


    /* cleanout log files older than LOG_CLEAN_PERIOD days */

    if (logRemoveDays > 0)
    {
        if ((cacheDirStream = opendir(options.ltoCacheDirectory)) == NULL)
        {
            fprintf(stderr, "Failed to open the cache directory to read its contents\n");
            return 1;
        }

        now = time(NULL);
        while ((cacheDirent = readdir(cacheDirStream)) != NULL)
        {
            strcpy(logFilename, options.ltoCacheDirectory);
            strcat_separator(logFilename);
            strcat(logFilename, cacheDirent->d_name);

            if (strncmp(g_logFilePrefix, cacheDirent->d_name, strlen(g_logFilePrefix)) == 0 &&
                strcmp(".txt", &cacheDirent->d_name[strlen(cacheDirent->d_name) - strlen(".txt")]) == 0 &&
                stat(logFilename, &statBuf) == 0 &&
                !S_ISDIR(statBuf.st_mode) &&
                now - statBuf.st_mtime > (logRemoveDays * 24 * 60 * 60))
            {
                strcpy(rmCmd, "rm -f ");
                strcat(rmCmd, logFilename);
                system(rmCmd);
            }
        }

        closedir(cacheDirStream);
    }


    /* open the log file */

    get_short_timestamp_string(timestampStr);
    strcpy(logFilename, options.ltoCacheDirectory);
    strcat_separator(logFilename);
    strcat(logFilename, g_logFilePrefix);
    strcat(logFilename, timestampStr);
    strcat(logFilename, ".txt");

    ml_set_log_level(logLevel);

    if (!ml_log_file_open(logFilename))
    {
        fprintf(stderr, "Failed to open log file '%s'\n", logFilename);
        return 1;
    }

    ml_log_info("Version: %s, build: %s\n", get_player_version(), get_player_build_timestamp());
    printf("Version: %s, build: %s\n", get_player_version(), get_player_build_timestamp());

    ml_log_info("Commandline options: ");
    printf("Commandline options: ");
    for (i = 1; i < argc; i++)
    {
        ml_log_info_cont(argv[i]);
        ml_log_info_cont(" ");
        printf(argv[i]);
        printf(" ");
    }
    ml_log_info_cont("\n");
    printf("\n");

    ml_log_info("Cache directory: '%s'\n", options.ltoCacheDirectory);
    printf("Cache directory: '%s'\n", options.ltoCacheDirectory);

    ml_log_file_flush();
    fflush(stdout);


    /* create QC Tape extract */

    if (!qce_create_lto_extract(options.ltoCacheDirectory, options.tapeDevice, &g_player.qcLTOExtract))
    {
        ml_log_error("Failed to create QC LTO Extract\n");
        goto fail;
    }

    /* create QC Tape access */

    if (!qla_create_qc_lto_access(options.ltoCacheDirectory, g_player.qcLTOExtract,
        options.deleteScriptName, options.deleteScriptOptions, &g_player.qcLTOAccess))
    {
        ml_log_error("Failed to create QC LTO Access\n");
        goto fail;
    }


    /* create buffer state log */

    if (bufferStateLogFilename != NULL)
    {
        if ((g_player.bufferStateLogFile = fopen(bufferStateLogFilename, "wb")) == NULL)
        {
            ml_log_error("Failed top open buffer state log file: %s\n", strerror(errno));
            goto fail;
        }

        fprintf(g_player.bufferStateLogFile, "# Columns: source buffers filled, sink buffers filled\n");
    }


    /* start the control threads */

    if (!start_control_threads(&g_player, &options))
    {
        ml_log_error("Failed to create terminal keyboard input\n");
        goto fail;
    }


#if defined(HAVE_SHTTPD)
    /* create the qc http access */

    if (options.qcHTTPPort >= 0)
    {
        if (!qch_create_qc_http_access(g_player.mediaPlayer, options.qcHTTPPort, options.ltoCacheDirectory,
            options.reportDirectory, &g_player.qcHTTPAccess))
        {
            ml_log_error("Failed to create QC http access\n");
            goto fail;
        }
    }
#endif


    /* start */

    qc_main(&g_player, argc, argv, &options);
    ml_log_info("QC player closed\n");


    /* wait for any tape seeks to complete */

    if (qce_is_seeking(g_player.qcLTOExtract))
    {
        printf("Waiting for the tape seek operation to complete...\n");
        ml_log_info("Waiting for the tape seek operation to complete...\n");
        while (qce_is_seeking(g_player.qcLTOExtract))
        {
            usleep(100000); /* 1/10th second */
        }
        printf("Tape seek operation completed\n");
        ml_log_info("Tape seek operation completed\n");
    }


    /* close down */

    ml_log_info("Exiting\n");
    ml_log_file_flush();
    printf("Exiting\n");
    fflush(stdout);
    cleanup_exit(0);
    return 0; /* for the benefit of the compiler */

fail:
    if (logFilename != NULL)
    {
        fprintf(stderr, "Failed - see %s for details\n", logFilename);
    }
    else
    {
        fprintf(stderr, "Failed\n");
    }
    cleanup_exit(1);
    return 1; /* for the benefit of the compiler */
}

