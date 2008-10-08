#ifndef __MOUSE_INPUT_CONNECT_H__
#define __MOUSE_INPUT_CONNECT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_control.h"
#include "video_switch_sink.h"
#include "mouse_input.h"


typedef struct MouseConnect MouseConnect;


int mic_create_mouse_connect(MediaControl* control, VideoSwitchSink* videoSwitch, 
    MouseInput* input, MouseConnect** connect);
void mic_free_mouse_connect(MouseConnect** connect);



#ifdef __cplusplus
}
#endif



#endif


