#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "dual_sink.h"
#include "x11_xv_display_sink.h"
#include "x11_display_sink.h"
#include "buffered_media_sink.h"
#include "dvs_sink.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"



typedef struct _X11Info
{
    struct _X11Info* next;
    
    int streamId;
    int acceptedFrame;
    unsigned char* buffer;
    unsigned int bufferSize;
} X11Info;

struct DualSink 
{
    int reviewDuration;
    int dvsCard;
    int dvsChannel;
    SDIVITCSource sdiVITCSource;
    int extraSDIVITCSource;
    int numBuffers;
    int disableSDIOSD;
    int disableX11OSD;
    int fitVideo;
    
    MediaSink mediaSink;
    OnScreenDisplay dualOSD;

    MediaSinkListener* listener;
    
    /* don't delete these; delete x11Sink instead */
    BufferedMediaSink* x11BufferedSink; 
    X11XVDisplaySink* x11XVDisplaySink;
    X11DisplaySink* x11DisplaySink;
    
    MediaSink* x11Sink;
    MediaSinkListener x11Listener; /* listen for refresh required events */
    MediaSink* dvsSink; 
    DVSSink* dvsDVSSink; 
    
    X11Info x11Info;
};


/* make sure the DVS is open - a call to reset_or_close could have closed it */
static int check_dvs_is_open(DualSink* dualSink)
{
    DVSSink* dvsSink = NULL;
    
    if (dualSink->dvsSink != NULL)
    {
        return 1;
    }

    CHK_ORET(dvs_open(dualSink->dvsCard, dualSink->dvsChannel, dualSink->sdiVITCSource, 
        dualSink->extraSDIVITCSource, dualSink->numBuffers, 
        dualSink->disableSDIOSD, dualSink->fitVideo, &dvsSink));
    dualSink->dvsSink = dvs_get_media_sink(dvsSink);
    
    return 1;
}

static int add_x11_info(DualSink* dualSink, int streamId)
{
    X11Info* info = &dualSink->x11Info;
    X11Info* prevInfo = NULL;
    X11Info* newInfo = NULL;
    
    /* if already exists then return */
    while (info != NULL)
    {
        if (info->streamId == streamId)
        {
            return 1;
        }
        
        prevInfo = info;
        info = info->next;
    }

    /* handle empty list */
    if (prevInfo == NULL)
    {
        memset(info, 0, sizeof(X11Info));
        info->streamId = streamId;
        return 1;
    }

    /* new element in list */
    CALLOC_ORET(newInfo, X11Info, 1);
    newInfo->streamId = streamId;
    prevInfo->next = newInfo;
    return 1;
}

static void free_x11_info(DualSink* dualSink)
{
    X11Info* info = dualSink->x11Info.next;
    X11Info* nextInfo = NULL;
    
    while (info != NULL)
    {
        nextInfo = info->next;
        SAFE_FREE(&info);
        info = nextInfo;
    } 
    
    dualSink->x11Info.streamId = -1;
    dualSink->x11Info.next = NULL;
}

static void reset_x11_info(DualSink* dualSink)
{
    X11Info* info = dualSink->x11Info.next;
    
    while (info != NULL)
    {
        info->acceptedFrame = 0;
        info->buffer = NULL;
        info->bufferSize = 0;
        info = info->next;
    } 
}

static X11Info* get_x11_info(DualSink* dualSink, int streamId)
{
    X11Info* info = &dualSink->x11Info;
    
    while (info != NULL)
    {
        if (info->streamId == streamId)
        {
            return info;
        }
        info = info->next;
    }
    
    return NULL;
}


static int dusk_osd_create_menu_model(void* data, OSDMenuModel** menu)
{
    DualSink* dualSink = (DualSink*)data;
    int result = 0;
    
    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_create_menu_model(msk_get_osd(dualSink->dvsSink), menu);
    if (!result)
    {
        result = osd_create_menu_model(msk_get_osd(dualSink->x11Sink), menu);
    }
    
    return result;
}

static void dusk_osd_free_menu_model(void* data, OSDMenuModel** menu)
{
    DualSink* dualSink = (DualSink*)data;
    
    CHK_ORETV(check_dvs_is_open(dualSink));
    
    if (*menu != NULL)
    {
        osd_free_menu_model(msk_get_osd(dualSink->dvsSink), menu);
        if (*menu != NULL)
        {
            osd_free_menu_model(msk_get_osd(dualSink->x11Sink), menu);
        }
        else
        {
            /* model was free'ed, so we only remove it from the x11 OSD */
            osd_set_active_menu_model(msk_get_osd(dualSink->x11Sink), 0, NULL);
        }
    }
}

static void dusk_osd_set_active_menu_model(void* data, int updateMask, OSDMenuModel* menu)
{
    DualSink* dualSink = (DualSink*)data;
    
    CHK_ORETV(check_dvs_is_open(dualSink));
    
    if ((updateMask << 1) >> 1 != updateMask)
    {
        ml_log_error("Update mask for setting active menu model is too large in the dual sink\n");
        return;
    }
    
    /* DVS responds to updates masked by updateMask, X11 responds to updates masked by updateMask << 1 */
    osd_set_active_menu_model(msk_get_osd(dualSink->dvsSink), updateMask, menu);
    osd_set_active_menu_model(msk_get_osd(dualSink->x11Sink), updateMask << 1, menu);
}

static int dusk_osd_set_screen(void* data, OSDScreen screen)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_set_screen(msk_get_osd(dualSink->dvsSink), screen);
    result = osd_set_screen(msk_get_osd(dualSink->x11Sink), screen) || result;
    
    return result;
}

static int dusk_osd_next_screen(void* data)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_next_screen(msk_get_osd(dualSink->dvsSink));
    result = osd_next_screen(msk_get_osd(dualSink->x11Sink)) || result;
    
    return result;
}

static int dusk_osd_get_screen(void* data, OSDScreen* screen)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
   
    if (msk_get_osd(dualSink->dvsSink) != NULL)
    { 
        return osd_get_screen(msk_get_osd(dualSink->dvsSink), screen);
    }
    else
    {
        return osd_get_screen(msk_get_osd(dualSink->x11Sink), screen);
    }
}

static int dusk_osd_next_timecode(void* data)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_next_timecode(msk_get_osd(dualSink->dvsSink));
    result = osd_next_timecode(msk_get_osd(dualSink->x11Sink)) || result;
    
    return result;
}

static int dusk_osd_set_timecode(void* data, int index, int type, int subType)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_set_timecode(msk_get_osd(dualSink->dvsSink), index, type, subType);
    result = osd_set_timecode(msk_get_osd(dualSink->x11Sink), index, type, subType) || result;
    
    return result;
}

static int dusk_osd_set_play_state(void* data, OSDPlayState state, int value)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_set_play_state(msk_get_osd(dualSink->x11Sink), state, value);
    result = osd_set_play_state(msk_get_osd(dualSink->dvsSink), state, value) || result;
    
    return result;
}

static int dusk_osd_set_state(void* data, const OnScreenDisplayState* state)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_set_state(msk_get_osd(dualSink->x11Sink), state);
    result = osd_set_state(msk_get_osd(dualSink->dvsSink), state) || result;
    
    return result;
}

static void dusk_osd_set_minimum_audio_stream_level(void* data, double level)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_set_minimum_audio_stream_level(msk_get_osd(dualSink->x11Sink), level);
    osd_set_minimum_audio_stream_level(msk_get_osd(dualSink->dvsSink), level);
}

static void dusk_osd_set_audio_lineup_level(void* data, float level)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_set_audio_lineup_level(msk_get_osd(dualSink->x11Sink), level);
    osd_set_audio_lineup_level(msk_get_osd(dualSink->dvsSink), level);
}

static void dusk_osd_reset_audio_stream_levels(void* data)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_reset_audio_stream_levels(msk_get_osd(dualSink->x11Sink));
    osd_reset_audio_stream_levels(msk_get_osd(dualSink->dvsSink));
}

static int dusk_osd_register_audio_stream(void* data, int streamId)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    result = osd_register_audio_stream(msk_get_osd(dualSink->x11Sink), streamId);
    result = osd_register_audio_stream(msk_get_osd(dualSink->dvsSink), streamId) || result;

    return result;
}

static void dusk_osd_set_audio_stream_level(void* data, int streamId, double level)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_set_audio_stream_level(msk_get_osd(dualSink->x11Sink), streamId, level);
    osd_set_audio_stream_level(msk_get_osd(dualSink->dvsSink), streamId, level);
}

static void dusk_osd_set_audio_level_visibility(void* data, int visible)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_set_audio_level_visibility(msk_get_osd(dualSink->x11Sink), visible);
    osd_set_audio_level_visibility(msk_get_osd(dualSink->dvsSink), visible);
}

static void dusk_osd_toggle_audio_level_visibility(void* data)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    osd_toggle_audio_level_visibility(msk_get_osd(dualSink->x11Sink));
    osd_toggle_audio_level_visibility(msk_get_osd(dualSink->dvsSink));
}

static void dusk_osd_show_field_symbol(void* data, int enable)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_show_field_symbol(msk_get_osd(dualSink->x11Sink), enable);
    osd_show_field_symbol(msk_get_osd(dualSink->dvsSink), enable);
}

static void dusk_osd_set_mark_display(void* data, const MarkConfigs* markConfigs)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_set_mark_display(msk_get_osd(dualSink->x11Sink), markConfigs);
    osd_set_mark_display(msk_get_osd(dualSink->dvsSink), markConfigs);
}

static int dusk_osd_create_marks_model(void* data, OSDMarksModel** model)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORETV(check_dvs_is_open(dualSink));

    result = osd_create_marks_model(msk_get_osd(dualSink->x11Sink), model);
    if (!result)
    {
        result = osd_create_marks_model(msk_get_osd(dualSink->dvsSink), model);
    }
    return result;
}

static void dusk_osd_set_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    if ((updateMask << 1) >> 1 != updateMask)
    {
        ml_log_error("Update mask for setting marks model is too large in the dual sink\n");
        return;
    }
    
    /* DVS responds to updates masked by updateMask, X11 responds to updates masked by updateMask << 1 */
    osd_set_marks_model(msk_get_osd(dualSink->x11Sink), updateMask, model);
    osd_set_marks_model(msk_get_osd(dualSink->dvsSink), updateMask << 1, model);
}

static void dusk_osd_set_second_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    if ((updateMask << 1) >> 1 != updateMask)
    {
        ml_log_error("Update mask for setting marks model is too large in the dual sink\n");
        return;
    }
    
    /* DVS responds to updates masked by updateMask, X11 responds to updates masked by updateMask << 1 */
    osd_set_second_marks_model(msk_get_osd(dualSink->x11Sink), updateMask, model);
    osd_set_second_marks_model(msk_get_osd(dualSink->dvsSink), updateMask << 1, model);
}

static void dusk_osd_set_progress_bar_visibility(void* data, int visible)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_set_progress_bar_visibility(msk_get_osd(dualSink->x11Sink), visible);
    osd_set_progress_bar_visibility(msk_get_osd(dualSink->dvsSink), visible);
}

static float dusk_osd_get_position_in_progress_bar(void* data, int x, int y)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    /* this function only makes sense for the X11 sink */ 
    return osd_get_position_in_progress_bar(msk_get_osd(dualSink->x11Sink), x, y);
}

static void dusk_osd_highlight_progress_bar_pointer(void* data, int on)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_highlight_progress_bar_pointer(msk_get_osd(dualSink->x11Sink), on);
    osd_highlight_progress_bar_pointer(msk_get_osd(dualSink->dvsSink), on);
}

static void dusk_osd_set_active_progress_bar_marks(void* data, int index)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_set_active_progress_bar_marks(msk_get_osd(dualSink->x11Sink), index);
    osd_set_active_progress_bar_marks(msk_get_osd(dualSink->dvsSink), index);
}

static void dusk_osd_set_label(void* data, int xPos, int yPos, int imageWidth, int imageHeight, 
    int fontSize, Colour colour, int box, const char* label)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));

    osd_set_label(msk_get_osd(dualSink->x11Sink), xPos, yPos, imageWidth, imageHeight, fontSize, colour, box, label);
    osd_set_label(msk_get_osd(dualSink->dvsSink), xPos, yPos, imageWidth, imageHeight, fontSize, colour, box, label);
}




static void dusk_x11_refresh_required(void* data)
{
    DualSink* dualSink = (DualSink*)data;

    msl_refresh_required(dualSink->listener);
}

static void dusk_x11_osd_screen_changed(void* data, OSDScreen screen)
{
    DualSink* dualSink = (DualSink*)data;

    msl_osd_screen_changed(dualSink->listener, screen);
}


static int dusk_register_listener(void* data, MediaSinkListener* listener)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_register_listener(dualSink->x11Sink, &dualSink->x11Listener);
    result = result && msk_register_listener(dualSink->dvsSink, listener);
    
    if (result)
    {
        dualSink->listener = listener;
    }
    
    return result;
}

static void dusk_unregister_listener(void* data, MediaSinkListener* listener)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    if (dualSink->listener == listener)
    {
        msk_unregister_listener(dualSink->x11Sink, &dualSink->x11Listener);
        msk_unregister_listener(dualSink->dvsSink, listener);

        dualSink->listener = NULL;
    }
}

static int dusk_accept_stream(void* data, const StreamInfo* streamInfo)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    return msk_accept_stream(dualSink->dvsSink, streamInfo);
}

static int dusk_register_stream(void* data, int streamId, const StreamInfo* streamInfo)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_register_stream(dualSink->dvsSink, streamId, streamInfo);
    if (result)
    {
        if (msk_accept_stream(dualSink->x11Sink, streamInfo))
        {
            if (msk_register_stream(dualSink->x11Sink, streamId, streamInfo))
            {
                if (!add_x11_info(dualSink, streamId))
                {
                    ml_log_warn("Failed to add X11 info data struct\n");
                }
            }
        }
    }

    return result;
}

static int dusk_accept_stream_frame(void* data, int streamId, const FrameInfo* frameInfo)
{
    DualSink* dualSink = (DualSink*)data;
    int result;
    X11Info* x11Info = NULL;
    
    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_accept_stream_frame(dualSink->dvsSink, streamId, frameInfo);
    if (result)
    {
        x11Info = get_x11_info(dualSink, streamId);
        if (x11Info != NULL)
        {
            x11Info->acceptedFrame = msk_accept_stream_frame(dualSink->x11Sink, streamId, frameInfo);
        }
    }

    return result;
}

static int dusk_get_stream_buffer(void* data, int streamId, unsigned int bufferSize, unsigned char** buffer)
{
    DualSink* dualSink = (DualSink*)data;
    int result;
    X11Info* x11Info = NULL;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_get_stream_buffer(dualSink->dvsSink, streamId, bufferSize, buffer);
    if (result)
    {
        x11Info = get_x11_info(dualSink, streamId);
        if (x11Info != NULL && x11Info->acceptedFrame)
        {
            if (msk_get_stream_buffer(dualSink->x11Sink, streamId, bufferSize, &x11Info->buffer))
            {
                x11Info->bufferSize = bufferSize;
            }
        }
    }

    return result;
}

static int dusk_receive_stream_frame(void* data, int streamId, unsigned char* buffer, unsigned int bufferSize)
{
    DualSink* dualSink = (DualSink*)data;
    int result;
    X11Info* x11Info = NULL;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_receive_stream_frame(dualSink->dvsSink, streamId, buffer, bufferSize);
    if (result)
    {
        x11Info = get_x11_info(dualSink, streamId);
        if (x11Info != NULL && x11Info->buffer != NULL)
        {
            msk_receive_stream_frame_const(dualSink->x11Sink, streamId, buffer, bufferSize);
        }
    }

    return result;
}

static int dusk_receive_stream_frame_const(void* data, int streamId, const unsigned char* buffer, unsigned int bufferSize)
{
    DualSink* dualSink = (DualSink*)data;
    int result;
    X11Info* x11Info = NULL;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_receive_stream_frame_const(dualSink->dvsSink, streamId, buffer, bufferSize);
    if (result)
    {
        x11Info = get_x11_info(dualSink, streamId);
        if (x11Info != NULL)
        {
            msk_receive_stream_frame_const(dualSink->x11Sink, streamId, buffer, bufferSize);
        }
    }

    return result;
}

static int dusk_complete_frame(void* data, const FrameInfo* frameInfo)
{
    DualSink* dualSink = (DualSink*)data;
    int result;

    CHK_ORET(check_dvs_is_open(dualSink));
    
    result = msk_complete_frame(dualSink->dvsSink, frameInfo);
    msk_complete_frame(dualSink->x11Sink, frameInfo);

    reset_x11_info(dualSink);

    return result;
}

static void dusk_cancel_frame(void* data)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    msk_cancel_frame(dualSink->dvsSink);
    msk_cancel_frame(dualSink->x11Sink);

    reset_x11_info(dualSink);
}

static int dusk_get_buffer_state(void* data, int* numBuffers, int* numBuffersFilled)
{
    DualSink* dualSink = (DualSink*)data;

    CHK_ORETV(check_dvs_is_open(dualSink));
    
    return msk_get_buffer_state(dualSink->dvsSink, numBuffers, numBuffersFilled);
}

static OnScreenDisplay* dusk_get_osd(void* data)
{
    DualSink* dualSink = (DualSink*)data;
    
    return &dualSink->dualOSD;
}

static void dusk_close(void* data)
{
    DualSink* dualSink = (DualSink*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    msk_close(dualSink->x11Sink);
    msk_close(dualSink->dvsSink);
    
    free_x11_info(dualSink);
    
    SAFE_FREE(&dualSink);
}

static int dusk_reset_or_close(void* data)
{
    DualSink* dualSink = (DualSink*)data;
    int result;
    
    result = msk_reset_or_close(dualSink->dvsSink);
    if (result == 0) /* failed */
    {
        goto fail;
    }
    else if (result == 2) /* was closed */
    {
        /* DVS was closed - we expect this to happen */
        dualSink->dvsSink = NULL;
    }
    
    result = msk_reset_or_close(dualSink->x11Sink);
    if (result != 1) /* not reset */
    {
        if (result == 2) /* was closed */
        {
            dualSink->x11Sink = NULL;
        }
        goto fail;
    }
     
    return 1;
    
fail:
    dusk_close(data);
    return 2;
}


int dusk_open(int reviewDuration, int dvsCard, int dvsChannel, SDIVITCSource sdiVITCSource, int extraSDIVITCSource, int numBuffers, 
    int useXV, int disableSDIOSD, int disableX11OSD, const Rational* pixelAspectRatio, 
    const Rational* monitorAspectRatio, float scale, int swScale, int fitVideo, X11PluginWindowInfo *pluginInfo, DualSink** dualSink)
{
    DualSink* newDualSink = NULL;
    
    CALLOC_ORET(newDualSink, DualSink, 1);

    newDualSink->reviewDuration = reviewDuration;
    newDualSink->dvsCard = dvsCard;
    newDualSink->dvsChannel = dvsChannel;
    newDualSink->sdiVITCSource = sdiVITCSource;
    newDualSink->extraSDIVITCSource = extraSDIVITCSource;
    newDualSink->numBuffers = numBuffers;
    newDualSink->disableSDIOSD = disableSDIOSD;
    newDualSink->disableX11OSD = disableX11OSD;
    newDualSink->fitVideo = fitVideo;
    
    if (useXV)
    {
        /* open buffered X11 XV display sink */    
        CHK_OFAIL(xvsk_open(reviewDuration, disableX11OSD, pixelAspectRatio, monitorAspectRatio, 
            scale, swScale, pluginInfo, &newDualSink->x11XVDisplaySink));
        newDualSink->x11Sink = xvsk_get_media_sink(newDualSink->x11XVDisplaySink);
    }
    else
    {
        /* open buffered X11 display sink */    
        CHK_OFAIL(xsk_open(reviewDuration, disableX11OSD, pixelAspectRatio, monitorAspectRatio, 
            scale, swScale, pluginInfo, &newDualSink->x11DisplaySink));
        newDualSink->x11Sink = xsk_get_media_sink(newDualSink->x11DisplaySink);
    }

    /* open buffered display sink */    
    CHK_OFAIL(bms_create(&newDualSink->x11Sink, numBuffers, 1, &newDualSink->x11BufferedSink));
    newDualSink->x11Sink = bms_get_sink(newDualSink->x11BufferedSink);

        
    /* open DVS display sink */    
    CHK_OFAIL(dvs_open(dvsCard, dvsChannel, sdiVITCSource, extraSDIVITCSource, numBuffers, 
        disableSDIOSD, fitVideo, &newDualSink->dvsDVSSink));
    newDualSink->dvsSink = dvs_get_media_sink(newDualSink->dvsDVSSink);
    
    
    newDualSink->mediaSink.data = newDualSink;
    newDualSink->mediaSink.register_listener = dusk_register_listener;
    newDualSink->mediaSink.unregister_listener = dusk_unregister_listener;
    newDualSink->mediaSink.accept_stream = dusk_accept_stream;
    newDualSink->mediaSink.register_stream = dusk_register_stream;
    newDualSink->mediaSink.accept_stream_frame = dusk_accept_stream_frame;
    newDualSink->mediaSink.get_stream_buffer = dusk_get_stream_buffer;
    newDualSink->mediaSink.receive_stream_frame = dusk_receive_stream_frame;
    newDualSink->mediaSink.receive_stream_frame_const = dusk_receive_stream_frame_const;
    newDualSink->mediaSink.complete_frame = dusk_complete_frame;
    newDualSink->mediaSink.cancel_frame = dusk_cancel_frame;
    newDualSink->mediaSink.get_buffer_state = dusk_get_buffer_state;
    newDualSink->mediaSink.get_osd = dusk_get_osd;
    newDualSink->mediaSink.reset_or_close = dusk_reset_or_close;
    newDualSink->mediaSink.close = dusk_close;

    newDualSink->x11Listener.data = newDualSink;
    newDualSink->x11Listener.refresh_required = dusk_x11_refresh_required;
    newDualSink->x11Listener.osd_screen_changed = dusk_x11_osd_screen_changed;

    newDualSink->dualOSD.data = newDualSink;
    newDualSink->dualOSD.create_menu_model = dusk_osd_create_menu_model;
    newDualSink->dualOSD.free_menu_model = dusk_osd_free_menu_model;
    newDualSink->dualOSD.set_active_menu_model = dusk_osd_set_active_menu_model;
    newDualSink->dualOSD.set_screen = dusk_osd_set_screen;
    newDualSink->dualOSD.next_screen = dusk_osd_next_screen;
    newDualSink->dualOSD.get_screen = dusk_osd_get_screen;
    newDualSink->dualOSD.next_timecode = dusk_osd_next_timecode;
    newDualSink->dualOSD.set_timecode = dusk_osd_set_timecode;
    newDualSink->dualOSD.set_play_state = dusk_osd_set_play_state;
    newDualSink->dualOSD.set_state = dusk_osd_set_state;
    newDualSink->dualOSD.set_minimum_audio_stream_level = dusk_osd_set_minimum_audio_stream_level;
    newDualSink->dualOSD.set_audio_lineup_level = dusk_osd_set_audio_lineup_level;
    newDualSink->dualOSD.reset_audio_stream_levels = dusk_osd_reset_audio_stream_levels;
    newDualSink->dualOSD.register_audio_stream = dusk_osd_register_audio_stream;
    newDualSink->dualOSD.set_audio_stream_level = dusk_osd_set_audio_stream_level;
    newDualSink->dualOSD.set_audio_level_visibility = dusk_osd_set_audio_level_visibility;
    newDualSink->dualOSD.toggle_audio_level_visibility = dusk_osd_toggle_audio_level_visibility;
    newDualSink->dualOSD.show_field_symbol = dusk_osd_show_field_symbol;
    newDualSink->dualOSD.set_mark_display = dusk_osd_set_mark_display;
    newDualSink->dualOSD.create_marks_model = dusk_osd_create_marks_model;
    newDualSink->dualOSD.set_marks_model = dusk_osd_set_marks_model;
    newDualSink->dualOSD.set_second_marks_model = dusk_osd_set_second_marks_model;
    newDualSink->dualOSD.set_progress_bar_visibility = dusk_osd_set_progress_bar_visibility;
    newDualSink->dualOSD.get_position_in_progress_bar = dusk_osd_get_position_in_progress_bar;
    newDualSink->dualOSD.highlight_progress_bar_pointer = dusk_osd_highlight_progress_bar_pointer;
    newDualSink->dualOSD.set_active_progress_bar_marks = dusk_osd_set_active_progress_bar_marks;
    newDualSink->dualOSD.set_label = dusk_osd_set_label;

    newDualSink->x11Info.streamId = -1;
    
    *dualSink = newDualSink;
    return 1;
    
fail:
    dusk_close(newDualSink);
    return 0;
}

MediaSink* dusk_get_media_sink(DualSink* dualSink)
{
    return &dualSink->mediaSink;
}

void dusk_set_media_control(DualSink* dualSink, ConnectMapping mapping, VideoSwitchSink* videoSwitch, MediaControl* control)
{
    if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_set_media_control(dualSink->x11XVDisplaySink, mapping, videoSwitch, control);
    }
    else if (dualSink->x11DisplaySink != NULL)
    {
        xsk_set_media_control(dualSink->x11DisplaySink, mapping, videoSwitch, control);
    }
}

void dusk_unset_media_control(DualSink* dualSink)
{
    if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_unset_media_control(dualSink->x11XVDisplaySink);
    }
    else if (dualSink->x11DisplaySink != NULL)
    {
        xsk_unset_media_control(dualSink->x11DisplaySink);
    }
}

void dusk_set_x11_window_name(DualSink* dualSink, const char* name)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_set_window_name(dualSink->x11DisplaySink, name);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_set_window_name(dualSink->x11XVDisplaySink, name);
    }
}

void dusk_register_window_listener(DualSink* dualSink, X11WindowListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_register_window_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_register_window_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_unregister_window_listener(DualSink* dualSink, X11WindowListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_unregister_window_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_unregister_window_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_register_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_register_keyboard_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_register_keyboard_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_unregister_keyboard_listener(DualSink* dualSink, KeyboardInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_unregister_keyboard_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_unregister_keyboard_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_register_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_register_progress_bar_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_register_progress_bar_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_unregister_progress_bar_listener(DualSink* dualSink, ProgressBarInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_unregister_progress_bar_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_unregister_progress_bar_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_register_mouse_listener(DualSink* dualSink, MouseInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_register_mouse_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_register_mouse_listener(dualSink->x11XVDisplaySink, listener);
    }
}

void dusk_unregister_mouse_listener(DualSink* dualSink, MouseInputListener* listener)
{
    if (dualSink->x11DisplaySink != NULL)
    {
        xsk_unregister_mouse_listener(dualSink->x11DisplaySink, listener);
    }
    else if (dualSink->x11XVDisplaySink != NULL)
    {
        xvsk_unregister_mouse_listener(dualSink->x11XVDisplaySink, listener);
    }
}


