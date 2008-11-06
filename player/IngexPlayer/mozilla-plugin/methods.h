 
#ifndef __METHODS_H__
#define __METHODS_H__


enum {
  METHOD_ID_None = -1,

  METHOD_ID_play = 0, // WMP
  METHOD_ID_Play, // Quicktime
  METHOD_ID_DoPlay, // Real Player
  METHOD_ID_pause, // WMP
  METHOD_ID_Pause, // Xine
  METHOD_ID_DoPause, // Real Player
  METHOD_ID_stop, // WMP
  METHOD_ID_Stop, // Quicktime
  METHOD_ID_DoStop, // Real Player
  METHOD_ID_CanPlay, // Real Player
  METHOD_ID_CanPause, // Real Player
  METHOD_ID_CanStop, // Real Player
  METHOD_ID_GetLength, // Real Player
  METHOD_ID_GetDuration, // Quicktime
  METHOD_ID_GetPosition, // Real Player
  METHOD_ID_GetCurrentPosition, // WMP
  METHOD_ID_GetTime, // Quicktime
  METHOD_ID_SetPosition, // Real Player
  METHOD_ID_SetCurrentPosition, // WMP
  METHOD_ID_SetTime, // Quicktime
  METHOD_ID_Rewind, // Quicktime
  METHOD_ID_GetStartTime, // Quicktime
  METHOD_ID_SetStartTime, // Quicktime
  METHOD_ID_GetTimeScale, // Quicktime
  METHOD_ID_GetPlayState, // Real Player
  METHOD_ID_GetSource, // Real Player
  METHOD_ID_GetURL, // Quicktime
  METHOD_ID_GetFileName, // WMP
  METHOD_ID_SetSource, // Real Player
  METHOD_ID_SetURL, // Quicktime
  METHOD_ID_SetFileName, // WMP
  METHOD_ID_GetAutoStart, // Real Player
  METHOD_ID_GetAutoPlay, // Quicktime
  METHOD_ID_SetAutoStart, // Real Player
  METHOD_ID_SetAutoPlay, // Quicktime
  METHOD_ID_GetLoop, // Real Player
  METHOD_ID_GetIsLooping, // Quicktime
  METHOD_ID_SetLoop, // Real Player
  METHOD_ID_SetIsLooping,// Quicktime
  METHOD_ID_GetNumLoop, // Real Player
  METHOD_ID_GetPlayCount, // WMP
  METHOD_ID_SetNumLoop, // Real Player
  METHOD_ID_SetPlayCount, // WMP
  METHOD_ID_GetClipWidth, // Real Player
  METHOD_ID_GetClipHeight, // Real Player
  METHOD_ID_HasNextEntry, // Real Player
  METHOD_ID_HasPrevEntry, // Real Player
  METHOD_ID_DoNextEntry, // Real Player
  METHOD_ID_DoPrevEntry, // Real Player
  METHOD_ID_GetNumEntries, // Real Player
  METHOD_ID_GetCurrentEntry, // Real Player
  METHOD_ID_SetCurrentEntry, // Xine
  
  NUM_METHODS
};


static const char *const methodName[NUM_METHODS] = {
"play"
};


#endif /* __METHODS_H__ */
