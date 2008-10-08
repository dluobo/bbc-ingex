#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mouse_input_connect.h"
#include "logging.h"
#include "macros.h"


struct MouseConnect
{
    MediaControl* control;
    VideoSwitchSink* videoSwitch;
    MouseInput* input;
    MouseInputListener listener;
};

static void mic_click(void* data, int imageWidth, int imageHeight, int xPos, int yPos)
{
    MouseConnect* connect = (MouseConnect*)data;
    int index;
    
    if (vsw_get_video_index(connect->videoSwitch, imageWidth, imageHeight, xPos, yPos, &index))
    {
        mc_switch_video(connect->control, index);
    }
}

int mic_create_mouse_connect(MediaControl* control, VideoSwitchSink* videoSwitch, 
    MouseInput* input, MouseConnect** connect)
{
    MouseConnect* newConnect;

    CALLOC_ORET(newConnect, MouseConnect, 1);
    
    newConnect->control = control;
    newConnect->videoSwitch = videoSwitch;
    newConnect->input = input;

    newConnect->listener.data = newConnect;
    newConnect->listener.click = mic_click;

    mip_set_listener(input, &newConnect->listener);
    
    *connect = newConnect;
    return 1;
}

void mic_free_mouse_connect(MouseConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }
    
    mip_unset_listener((*connect)->input);
    
    SAFE_FREE(connect);
}



