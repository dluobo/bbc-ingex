#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include <pthread.h>

#ifdef _POSIX_PRIORITY_SCHEDULING
# include <sched.h>
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>

#include "npapi.h"
#include "npruntime.h"

#include "LocalIngexPlayer.h"

#include "debug_x11.h"

using namespace prodauto;
using namespace std;

#if 1
#define log(x, ...) \
 do {\
      fprintf (stderr, "ingex-plugin (%d): " x "\n", getpid(), ##__VA_ARGS__);\
      fflush  (stderr);\
 } while (0)
#else
#define log(x, ...)
#endif

class TestIngexPlayerListener : public IngexPlayerListener
{
public:    
    TestIngexPlayerListener(LocalIngexPlayer* player)
        : IngexPlayerListener(player), _player(player) {};
    virtual ~TestIngexPlayerListener() {};

    virtual void frameDisplayedEvent(const FrameInfo* frameInfo)
    {
        printf("Frame %lld displayed\n", frameInfo->position);
    }
    
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo)
    {
        printf("Frame %lld dropped\n", lastFrameInfo->position);
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
        printf("End of source reached (%lld)\n", lastReadFrameInfo->position);
    }
    
    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo)
    {
        printf("Start of source reached (%lld)\n", firstReadFrameInfo->position);
    }
    
    virtual void playerCloseRequested()
    {
        printf("Player close requested\n");
    }
    
    virtual void playerClosed()
    {
        printf("Player has closed\n");
    }
    
    virtual void keyPressed(int key, int modifier)
    {
        printf("Key pressed %d (modifier=%d)\n", key, modifier);
        switch (key)
        {
            case 'q':
                _player->stop();
                break;
            case XK_space:
                _player->togglePlayPause();
                break;
            case XK_Right:
                _player->step(true);
                break;
            case XK_Left:
                _player->step(false);
                break;
            case XK_Up:
                /* 10 frames speed up forwards */
                _player->playSpeed(10);
                break;
            case XK_Down:
                /* 10 frames speed up backwards */
                _player->playSpeed(-10);
                break;
            case XK_Page_Up:
                /* step 1 minute backwards */
                _player->seek(-1500, SEEK_CUR, FRAME_PLAY_UNIT);
                break;
            case XK_Page_Down:
                /* step 1 minute forwards */
                _player->seek(1500, SEEK_CUR, FRAME_PLAY_UNIT);
                break;
            case XK_Home:
                _player->seek(0, SEEK_SET, FRAME_PLAY_UNIT);
                break;
            case XK_End:
                _player->seek(0, SEEK_END, FRAME_PLAY_UNIT);
                break;
            case 'o':
                _player->nextOSDScreen();
                break;
            case 't':
                _player->nextOSDTimecode();
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                _player->switchVideo(key - '0');
                break;
            case 'e':
                _player->switchPrevAudioGroup();
                break;
            case 'p':
                _player->snapAudioToVideo();
                break;
            case 'r':
                _player->switchNextAudioGroup();
                break;
            case 'v':
                _player->muteAudio(-1 /* toggle */);
                break;
            default:
                break;
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
};

/* Private per-instance data */
typedef struct {
  NPP                 instance;

  Display            *display;
  int                 screen;
  Window              parent;
  Window              window;
  int                 x, y;
  unsigned            w, h;

  char                mrl[FILENAME_MAX];

  X11WindowInfo pluginInfo;
  auto_ptr<LocalIngexPlayer> player;
  auto_ptr<TestIngexPlayerListener> listener;
  vector<PlayerInput> inputs;
  vector<bool> opened;

  pthread_mutex_t     mutex;
  pthread_t           thread;
  int                 playing;
  
#ifdef ENABLE_SCRIPTING
  NPObject           *object;
#endif
} ingex_plugin_t;


#ifdef ENABLE_SCRIPTING

#include "methods.h"

typedef struct {
  NPObject       base;
  
  ingex_plugin_t *plugin;
  
  NPIdentifier   methods[NUM_METHODS];
  int            methods_count;
} NPPObject;

static NPObject *NPPObject_Allocate (NPP instance, NPClass *npclass) {
  NPPObject *object;
  
  log ("NPPObject_Allocate( instance=%p, npclass=%p )", instance, npclass);
  
  if (!instance || !instance->pdata)
    return NULL;
  
  object = (NPPObject *)NPN_MemAlloc (sizeof(NPPObject));
  if (!object)
    return NULL;
    
  object->base._class = npclass;
  object->base.referenceCount = 1;
  object->plugin = (ingex_plugin_t*)instance->pdata;
  object->methods_count = 0;
  
  return &object->base;
}

static int NPPObject_GetMethodID (NPObject *npobj, NPIdentifier name) {
  NPPObject *object = (NPPObject *) npobj;
  int        i;
  
  for (i = 0; i < object->methods_count; i++) {
    if (object->methods[i] == name)
      return i;
  }
  
  for (; i < NUM_METHODS; i++) {
    object->methods[i] = NPN_GetStringIdentifier (methodName[i]);
    object->methods_count++;
    if (object->methods[i] == name)
      return i;
  }
  
  return METHOD_ID_None;
}

static bool NPPObject_HasMethod (NPObject *npobj, NPIdentifier name) {  
  log ("NPPObject_HasMethod( npobj=%p, name=%p )", npobj, name);
  
  if (NPN_IdentifierIsString (name) &&
      NPPObject_GetMethodID (npobj, name) != METHOD_ID_None)
    return true;
  
  return false;
}

static bool NPPObject_Invoke (NPObject *npobj, NPIdentifier name,
                              const NPVariant *args, uint32_t argCount,
                              NPVariant *result) {
  ingex_plugin_t *priv;
  int            method;
  int            ret = 0;
  
  log ("NPPObject_Invoke( npobj=%p, name=%p, args=%p, argCount=%u, result=%p )",
       npobj, name, args, argCount, result);
       
  priv = ((NPPObject *)npobj)->plugin;
  method = NPPObject_GetMethodID (npobj, name);
  
  switch (method) {
    case METHOD_ID_play:
      log ("Plugin::play()");
      break;
    default:
      log ("called unsupported method");
      return false;
  }
  
  INT32_TO_NPVARIANT (ret, *result);
  
  return true;
}

static bool NPPObject_InvokeDefault (NPObject *npobj, 
                                     const NPVariant *args, uint32_t argCount, 
                                     NPVariant *result) {
  log ("NPPObject_Invoke( npobj=%p, args=%p, argCount=%u )", npobj, args, argCount);
 
  return false;
}

/*
 * The following properties are only used by WMP.
 */

static bool NPPObject_HasProperty (NPObject *npobj, NPIdentifier name) {
  log ("NPPObject_HasProperty( npobj=%p, name=%p )", npobj, name);
  
  if (name == NPN_GetStringIdentifier ("controls")  ||
      name == NPN_GetStringIdentifier ("URL")       ||
      name == NPN_GetStringIdentifier ("SRC")       ||
      name == NPN_GetStringIdentifier ("Filename")  ||
      name == NPN_GetStringIdentifier ("autoStart") ||
      name == NPN_GetStringIdentifier ("playCount") ||
      name == NPN_GetStringIdentifier ("currentPosition"))
    return true;
  
  return false;
}

static bool NPPObject_GetProperty (NPObject *npobj, NPIdentifier name, NPVariant *result) {
  ingex_plugin_t *priv;
  
  log ("NPPObject_GetProperty( npobj=%p, name=%p )", npobj, name);
  
  priv = ((NPPObject *)npobj)->plugin;
  
  if (name == NPN_GetStringIdentifier ("controls")) {
    NPN_RetainObject (npobj);
    OBJECT_TO_NPVARIANT (npobj, *result);
    return true;
  }
 
  return false;
}

static bool NPPObject_SetProperty (NPObject *npobj, NPIdentifier name, const NPVariant *value) {
  ingex_plugin_t *priv;
  
  log ("NPPObject_SetProperty( npobj=%p, name=%p, value=%p )", npobj, name, value);
  
  priv = ((NPPObject *)npobj)->plugin;
  
  return false;
}

static bool NPPObject_RemoveProperty (NPObject *npobj, NPIdentifier name) {
  log ("NPPObject_RemoveProperty( npobj=%p, name=%p )", npobj, name);
  
  return false;
}

static NPClass pluginClass = {
  NP_CLASS_STRUCT_VERSION,
  NPPObject_Allocate,
  NULL,
  NULL,
  NPPObject_HasMethod,
  NPPObject_Invoke,
  NPPObject_InvokeDefault,
  NPPObject_HasProperty,
  NPPObject_GetProperty,
  NPPObject_SetProperty,
  NPPObject_RemoveProperty,
};
#endif /* ENABLE_SCRIPTING */

/*****************************************************************************/

/* Called only once after installation. */
char *NPP_GetMIMEDescription (void) {
  const char *dsc =
  					"application/x-ingex-player: *: Ingex Player Plugin;"
					/* rfc4539.txt specifies "application/mxf" */
  					"application/mxf: mxf: MXF wrapped video and audio";
  log("NPP_GetMIMEDescription() returning dsc=%s", dsc);

  return (char *)dsc;
}

NPError NPP_GetValue (NPP instance, NPPVariable variable, void *value) {
  log ("NPP_GetValue( instance=%p, variable=%d )", instance, variable);

  switch (variable) {
    case NPPVpluginNameString:
      log ("  NPPVpluginNameString");
      *((char **) value) = (char*)"Ingex Player Plugin";
      break;
    case NPPVpluginDescriptionString:
      log ("  NPPVpluginDescriptionString");
      *((char **) value) =  (char*)
                  "Ingex Player Plugin version 0.0.1, "          
                  "(c) <a href=http://ingex.sourceforge.net>BBC Research</a>.<br>";
      break;
    case NPPVpluginNeedsXEmbed:
      log ("  NPPVpluginNeedsXEmbed");
      *((NPBool *) value) = FALSE;
      break;
#ifdef ENABLE_SCRIPTING 
    case NPPVpluginScriptableNPObject:
      log ("  NPPVpluginScriptableNPObject");
      if (!instance || !instance->pdata)
        return NPERR_INVALID_INSTANCE_ERROR;
      
      priv = (ingex_plugin_t*)instance->pdata;
      if (!priv->object) {
        priv->object = NPN_CreateObject (instance, &pluginClass);
        if (!priv->object)
          return NPERR_OUT_OF_MEMORY_ERROR;
      }
      
      *((NPObject **) value) = NPN_RetainObject (priv->object);
      break;
#else
    case NPPVpluginScriptableIID:
		log("  NPPVpluginScriptableIID - not supported");
        return NPERR_GENERIC_ERROR;
		break;
    case NPPVpluginScriptableNPObject:
		log("  NPPVpluginScriptableNPObject - not enabled at build time");
        return NPERR_GENERIC_ERROR;
		break;
#endif /* ENABLE_SCRIPTING */ 
    default:
      log ("  default - return NPERR_GENERIC_ERROR");
      return NPERR_GENERIC_ERROR;
  }

  log ("  return NPERR_NO_ERROR");
  return NPERR_NO_ERROR;
}

NPError NPP_SetValue (NPP instance, NPNVariable variable, void *value) {
  log ("NPP_GetValue( instance=%p, variable=%d )", instance, variable);
  
  return NPERR_GENERIC_ERROR;
}

NPError NPP_Initialize (void) {
  log ("NPP_Initialize()");
  
  return NPERR_NO_ERROR;
}

NPError NPP_New (NPMIMEType mimetype, NPP instance, uint16 mode,
                 int16 argc, char *argn[], char *argv[], NPSavedData *saved) {
  ingex_plugin_t       *priv;
  pthread_mutexattr_t  attr;
  int                  i;
  PlayerInput          input;

  log ("NPP_New( mimetype=%s, instance=%p, mode=%d, saved=%p )",
       mimetype, instance, mode, saved);

  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  /* Allocate object to store instance-specific private data */
  priv = (ingex_plugin_t*)NPN_MemAlloc (sizeof(ingex_plugin_t));
  if (!priv)
    return NPERR_OUT_OF_MEMORY_ERROR;

  memset (priv, 0, sizeof(ingex_plugin_t));

  priv->instance = instance;

  /* Parse parameter list */
  for (i = 0; i < argc; i++) {
    log ("param(%s) = \"%s\"", argn[i], argv[i]);
    
    if (argn[i] == NULL)
      continue;

    if (!strcmp (argn[i], "mrl")) {
		if (! *argv[i])
			return NPERR_INVALID_PARAM;
        strncpy(priv->mrl, argv[i], sizeof(priv->mrl));
        input.type = MXF_INPUT;
        input.name = priv->mrl;
        input.options.clear(); 
		priv->inputs.push_back(input);
    }
  }

  priv->player = auto_ptr<LocalIngexPlayer>(new LocalIngexPlayer(X11_AUTO_OUTPUT));
  priv->listener = auto_ptr<TestIngexPlayerListener>(new TestIngexPlayerListener(priv->player.get()));

  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init (&priv->mutex, &attr);
  pthread_mutexattr_destroy (&attr);   

  instance->pdata = priv;

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow (NPP instance, NPWindow *window)
{
  ingex_plugin_t *priv;

  log ("NPP_SetWindow( instance=%p, window=%p )", instance, window);
  debug_x11_events( ((NPSetWindowCallbackStruct *)window->ws_info)->display, (Window)window->window );

  if (!instance || !instance->pdata)
    return NPERR_INVALID_INSTANCE_ERROR;

  priv = (ingex_plugin_t*)instance->pdata;

  if (!window || !window->window) {
    log ("  window destoyed?");
  } 
  else {
    log ("  window id = %lu", (Window)window->window);

    if (priv->window == 0) {
      log("  priv->window == 0");
	  /* Fill in X11 data */
	  priv->window = (Window)window->window;

      // To get X11 objects use ws_info
	  NPSetWindowCallbackStruct *ws_info = (NPSetWindowCallbackStruct *)window->ws_info;
	  Display *display = ws_info->display;

      // At this point firefox wants most events handled.
	  // Reset this with NoEventMask so ingex_player can add event handling.
      XSelectInput(display, priv->window, NoEventMask);
      debug_x11_events( display, (Window)window->window );

	  // Set Display to NULL to make ingex_player open new connection with XOpenDisplay.
	  // The separate connection will avoid having to use XLockDisplay, XUnlockDisplay.
	  priv->pluginInfo.display = NULL;
	  priv->pluginInfo.window = priv->window;
	  priv->pluginInfo.gc = 0;
	  priv->pluginInfo.deleteAtom = 0;
	  priv->player->setWindowInfo(&priv->pluginInfo);

	  priv->player->start(priv->inputs, priv->opened, false, 0);
    }
    else if (priv->w != window->width || priv->h != window->height) {
      log ("  window resized: %dx%d -> %dx%d.",
            priv->w, priv->h, (int) window->width, (int) window->height);
    }
    else {
      /* Expose */
      log("  Expose");
    }
  }

  return NPERR_NO_ERROR;
}

NPError NPP_NewStream (NPP instance, NPMIMEType mimetype,
                       NPStream *stream, NPBool seekable, uint16 *stype) {
  log ("NPP_NewStream( instance=%p, mimetype=%s, stream=%p, seekable=%d )",
       instance, mimetype, stream, seekable);

  if (!instance || !instance->pdata)
    return NPERR_INVALID_INSTANCE_ERROR;

  return NPERR_NO_ERROR;
}

int32 NPP_WriteReady (NPP instance, NPStream *stream) {
  log ("NPP_WriteReady( instance=%p, stream=%p )", instance, stream);

  return 0x0fffffff;
}

int32 NPP_Write (NPP instance, NPStream *stream,
                 int32 offset, int32 len, void *buffer) {
  log ("NPP_Write( instance=%p, stream=%p, offset=%d, len=%d, buffer=%p )",
       instance, stream, (int) offset, (int) len, buffer);

  return -1;
}

void NPP_StreamAsFile (NPP instance, NPStream *stream, const char *file) {
  log ("NPP_StreamAsFile( instance=%p, stream=%p, file=%s )",
       instance, stream, file);

  if (!instance || !instance->pdata)
    return;
  return;
}

NPError NPP_DestroyStream (NPP instance, NPStream *stream, NPReason reason) {
  log ("NPP_DestroyStream( instance=%p, stream=%p, reason=%d )",
       instance, stream, reason);

  if (!instance || !instance->pdata)
    return NPERR_INVALID_INSTANCE_ERROR;

  return NPERR_NO_ERROR;
}

void NPP_Print (NPP instance, NPPrint *printinfo) {
  log ("NPP_Print( instance=%p, printinfo=%p )", instance, printinfo);
}

void NPP_URLNotify (NPP instance, const char* url,
                    NPReason reason, void* notifyData) {
  log ("NPP_URLNotify( instance=%p, url=%s, reason=%d, notifyData=%p )",
       instance, url, reason, notifyData);
}

NPError NPP_Destroy (NPP instance, NPSavedData **save) {
  ingex_plugin_t *priv;

  log ("NPP_Destroy( instance=%p, save=%p )", instance, save);

  if (!instance || !instance->pdata)
    return NPERR_INVALID_INSTANCE_ERROR;

  priv = (ingex_plugin_t*)instance->pdata;
 
  priv->player->close();

#ifdef ENABLE_SCRIPTING 
  if (priv->object)
    NPN_ReleaseObject (priv->object);
#endif

  pthread_mutex_destroy (&priv->mutex);

  NPN_MemFree (priv);

  instance->pdata = NULL;

  return NPERR_NO_ERROR;
}

void NPP_Shutdown (void) {
  log ("NPP_Shutdown()");
}
