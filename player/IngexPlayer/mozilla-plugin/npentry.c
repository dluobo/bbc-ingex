/*
 * Copyright (C) 2000-2007 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * 
 * Xine plugin for Mozilla/Firefox 
 *      written by Claudio Ciccani <klan@users.sf.net>
 *      
 */

#include <stdio.h>
#include <string.h>

#include "npapi.h"
#include "npupp.h"

/* Build version */
#define NP_BUILD_VERSION  ((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR)

/* Minimum supported version */
#define NP_MIN_SUPPORTED_VERSION  13

#define NP_VERSION_HAS_RUNTIME    14

#define NP_VERSION_HAS_POPUP      16

/*****************************************************************************/

static NPNetscapeFuncs NPNFuncs; /* Global functions table. */

/*****************************************************************************/

void NPN_Version (int* plugin_major, int* plugin_minor,
                  int* netscape_major, int* netscape_minor)
{
    *plugin_major = NP_VERSION_MAJOR;
    *plugin_minor = NP_VERSION_MINOR;
    *netscape_major = NPNFuncs.version >> 8;
    *netscape_minor = NPNFuncs.version & 0xFF;
}

NPError NPN_GetValue (NPP instance, NPNVariable variable, void *r_value) 
{
  return CallNPN_GetValueProc (NPNFuncs.getvalue, instance, variable, r_value);
}

NPError NPN_SetValue (NPP instance, NPPVariable variable, void *value)
{
  return CallNPN_SetValueProc (NPNFuncs.setvalue, instance, variable, value);
}

NPError NPN_GetURL (NPP instance, const char* url, const char* window)
{
  return CallNPN_GetURLProc (NPNFuncs.geturl, instance, url, window);
}

NPError NPN_GetURLNotify (NPP instance, const char* url, 
                          const char* window, void* notifyData)
{
  return CallNPN_GetURLNotifyProc (NPNFuncs.geturlnotify, instance,
                                   url, window, notifyData);
}

NPError NPN_PostURL (NPP instance, const char* url, const char* window,
                     uint32 len, const char* buf, NPBool file)
{
  return CallNPN_PostURLProc (NPNFuncs.posturl, instance, url, window, len, buf, file);
}

NPError NPN_PostURLNotify (NPP instance, const char* url, const char* window, 
                           uint32 len, const char* buf, NPBool file, void* notifyData)
{
  return CallNPN_PostURLNotifyProc (NPNFuncs.posturlnotify, instance, url,
                                   window, len, buf, file, notifyData);
}

NPError NPN_RequestRead (NPStream* stream, NPByteRange* rangeList)
{
  return CallNPN_RequestReadProc (NPNFuncs.requestread, stream, rangeList);
}

NPError NPN_NewStream (NPP instance, NPMIMEType type, 
                       const char *window, NPStream** stream_ptr)
{
  return CallNPN_NewStreamProc (NPNFuncs.newstream, instance, 
                                type, window, stream_ptr);
}

int32 NPN_Write (NPP instance, NPStream* stream, int32 len, void* buffer)
{
  return CallNPN_WriteProc (NPNFuncs.write, instance, stream, len, buffer);
}

NPError NPN_DestroyStream (NPP instance, NPStream* stream, NPError reason)
{
  return CallNPN_DestroyStreamProc (NPNFuncs.destroystream, instance, stream, reason);
}

void NPN_Status (NPP instance, const char* message)
{
  /* Browsers different from Firefox might not be thread-safe. */
  if (strstr (NPN_UserAgent(instance), "Firefox"))
    CallNPN_StatusProc (NPNFuncs.status, instance, message);
}

const char* NPN_UserAgent (NPP instance)
{
  return CallNPN_UserAgentProc (NPNFuncs.uagent, instance);
}

void* NPN_MemAlloc (uint32 size)
{
  return CallNPN_MemAllocProc (NPNFuncs.memalloc, size);
}

void NPN_MemFree (void* ptr)
{
  CallNPN_MemFreeProc (NPNFuncs.memfree, ptr);
}

uint32 NPN_MemFlush (uint32 size)
{
  return CallNPN_MemFlushProc (NPNFuncs.memflush, size);
}

void NPN_ReloadPlugins (NPBool reloadPages)
{
  CallNPN_ReloadPluginsProc (NPNFuncs.reloadplugins, reloadPages);
}

void NPN_InvalidateRect (NPP instance, NPRect *invalidRect)
{
  CallNPN_InvalidateRectProc (NPNFuncs.invalidaterect, instance, invalidRect);
}

void
NPN_InvalidateRegion (NPP instance, NPRegion invalidRegion)
{
  CallNPN_InvalidateRegionProc (NPNFuncs.invalidateregion, instance, invalidRegion);
}

void
NPN_ForceRedraw (NPP instance)
{
  CallNPN_ForceRedrawProc (NPNFuncs.forceredraw, instance);
}

NPIdentifier NPN_GetStringIdentifier (const NPUTF8 *name)
{
  return CallNPN_GetStringIdentifierProc (NPNFuncs.getstringidentifier, name);
}

void NPN_GetStringIdentifiers (const NPUTF8 **names, int32_t nameCount,
                               NPIdentifier *identifiers)
{
  CallNPN_GetStringIdentifiersProc (NPNFuncs.getstringidentifiers,
                                    names, nameCount, identifiers);
}

NPIdentifier NPN_GetIntIdentifier (int32_t intid)
{
  return CallNPN_GetIntIdentifierProc (NPNFuncs.getintidentifier, intid);
}

bool NPN_IdentifierIsString (NPIdentifier identifier)
{
  return CallNPN_IdentifierIsStringProc (NPNFuncs.identifierisstring, identifier);
}

NPUTF8 *NPN_UTF8FromIdentifier (NPIdentifier identifier)
{
  return CallNPN_UTF8FromIdentifierProc (NPNFuncs.utf8fromidentifier, identifier);
}

int32_t NPN_IntFromIdentifier (NPIdentifier identifier)
{
  return CallNPN_IntFromIdentifierProc (NPNFuncs.intfromidentifier, identifier);
}

NPObject *NPN_CreateObject (NPP npp, NPClass *aClass)
{
  return CallNPN_CreateObjectProc (NPNFuncs.createobject, npp, aClass);
}

NPObject *NPN_RetainObject (NPObject *obj)
{
  return CallNPN_RetainObjectProc (NPNFuncs.retainobject, obj);
}

void NPN_ReleaseObject (NPObject *obj)
{
  return CallNPN_ReleaseObjectProc (NPNFuncs.releaseobject, obj);
}

bool NPN_Invoke (NPP npp, NPObject* obj, NPIdentifier methodName,
                 const NPVariant *args, uint32_t argCount, NPVariant *result)
{
  return CallNPN_InvokeProc (NPNFuncs.invoke, npp, obj, methodName, args, argCount, result);
}

bool NPN_InvokeDefault (NPP npp, NPObject* obj, const NPVariant *args,
                        uint32_t argCount, NPVariant *result)
{
  return CallNPN_InvokeDefaultProc (NPNFuncs.invokeDefault, npp, obj, args, argCount, result);
}

bool NPN_Evaluate (NPP npp, NPObject* obj, NPString *script, NPVariant *result)
{
  return CallNPN_EvaluateProc (NPNFuncs.evaluate, npp, obj, script, result);
}

bool NPN_GetProperty (NPP npp, NPObject* obj, NPIdentifier propertyName, NPVariant *result)
{
  return CallNPN_GetPropertyProc (NPNFuncs.getproperty, npp, obj, propertyName, result);
}

bool NPN_SetProperty (NPP npp, NPObject* obj, NPIdentifier propertyName,
                      const NPVariant *value)
{
  return CallNPN_SetPropertyProc (NPNFuncs.setproperty, npp, obj, propertyName, value);
}

bool NPN_RemoveProperty (NPP npp, NPObject* obj, NPIdentifier propertyName)
{
  return CallNPN_RemovePropertyProc (NPNFuncs.removeproperty, npp, obj, propertyName);
}

bool NPN_HasProperty (NPP npp, NPObject* obj, NPIdentifier propertyName)
{
  return CallNPN_HasPropertyProc (NPNFuncs.hasproperty, npp, obj, propertyName);
}

bool NPN_HasMethod (NPP npp, NPObject* obj, NPIdentifier methodName)
{
  return CallNPN_HasMethodProc (NPNFuncs.hasmethod, npp, obj, methodName);
}

void NPN_ReleaseVariantValue(NPVariant *variant)
{
  CallNPN_ReleaseVariantValueProc (NPNFuncs.releasevariantvalue, variant);
}

void NPN_SetException(NPObject* obj, const NPUTF8 *message)
{
  CallNPN_SetExceptionProc (NPNFuncs.setexception, obj, message);
}

void NPN_PushPopupsEnabledState (NPP instance, NPBool enabled)
{
  CallNPN_PushPopupsEnabledStateProc (NPNFuncs.pushpopupsenabledstate, instance, enabled);
}

void NPN_PopPopupsEnabledState (NPP instance)
{
  CallNPN_PopPopupsEnabledStateProc (NPNFuncs.poppopupsenabledstate, instance);
}

/*****************************************************************************/

char*
NP_GetMIMEDescription (void)
{
    return NPP_GetMIMEDescription ();
}

NPError
NP_GetValue (void *future, NPPVariable variable, void *value)
{
    return NPP_GetValue (future, variable, value);
}

NPError OSCALL
#ifdef XP_UNIX
NP_Initialize (NPNetscapeFuncs *pFuncs, NPPluginFuncs *pluginFuncs)
#else
NP_Initialize (NPNetscapeFuncs *pFuncs)
#endif
{
  if (pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;
    
  if ((pFuncs->version>>8) > NP_VERSION_MAJOR ||
       pFuncs->version < NP_MIN_SUPPORTED_VERSION) {
    fprintf (stderr, "xine-plugin: incompatible NPAPI version (%d.%02d)!\n",
             pFuncs->version >> 8, pFuncs->version & 0xff);
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  NPNFuncs.size                    = pFuncs->size;
  NPNFuncs.version                 = pFuncs->version;
  NPNFuncs.geturlnotify            = pFuncs->geturlnotify;
  NPNFuncs.geturl                  = pFuncs->geturl;
  NPNFuncs.posturlnotify           = pFuncs->posturlnotify;
  NPNFuncs.posturl                 = pFuncs->posturl;
  NPNFuncs.requestread             = pFuncs->requestread;
  NPNFuncs.newstream               = pFuncs->newstream;
  NPNFuncs.write                   = pFuncs->write;
  NPNFuncs.destroystream           = pFuncs->destroystream;
  NPNFuncs.status                  = pFuncs->status;
  NPNFuncs.uagent                  = pFuncs->uagent;
  NPNFuncs.memalloc                = pFuncs->memalloc;
  NPNFuncs.memfree                 = pFuncs->memfree;
  NPNFuncs.memflush                = pFuncs->memflush;
  NPNFuncs.reloadplugins           = pFuncs->reloadplugins;
  NPNFuncs.getJavaEnv              = pFuncs->getJavaEnv;
  NPNFuncs.getJavaPeer             = pFuncs->getJavaPeer;
  NPNFuncs.getvalue                = pFuncs->getvalue;
  NPNFuncs.setvalue                = pFuncs->setvalue;
  NPNFuncs.invalidaterect          = pFuncs->invalidaterect;
  NPNFuncs.invalidateregion        = pFuncs->invalidateregion;
  NPNFuncs.forceredraw             = pFuncs->forceredraw;
  if (pFuncs->version >= NP_VERSION_HAS_RUNTIME) {
    NPNFuncs.getstringidentifier    = pFuncs->getstringidentifier;
    NPNFuncs.getstringidentifiers   = pFuncs->getstringidentifiers;
    NPNFuncs.getintidentifier       = pFuncs->getintidentifier;
    NPNFuncs.identifierisstring     = pFuncs->identifierisstring;
    NPNFuncs.utf8fromidentifier     = pFuncs->utf8fromidentifier;
    NPNFuncs.intfromidentifier      = pFuncs->intfromidentifier;
    NPNFuncs.createobject           = pFuncs->createobject;
    NPNFuncs.retainobject           = pFuncs->retainobject;
    NPNFuncs.releaseobject          = pFuncs->releaseobject;
    NPNFuncs.invoke                 = pFuncs->invoke;
    NPNFuncs.invokeDefault          = pFuncs->invokeDefault;
    NPNFuncs.evaluate               = pFuncs->evaluate;
    NPNFuncs.getproperty            = pFuncs->getproperty;
    NPNFuncs.setproperty            = pFuncs->setproperty;
    NPNFuncs.removeproperty         = pFuncs->removeproperty;
    NPNFuncs.hasproperty            = pFuncs->hasproperty;
    NPNFuncs.hasmethod              = pFuncs->hasmethod;
    NPNFuncs.releasevariantvalue    = pFuncs->releasevariantvalue;
    NPNFuncs.setexception           = pFuncs->setexception;
  }
  if (pFuncs->version >= NP_VERSION_HAS_POPUP) {
    NPNFuncs.pushpopupsenabledstate = pFuncs->pushpopupsenabledstate;
    NPNFuncs.poppopupsenabledstate  = pFuncs->poppopupsenabledstate;
  }

#ifdef XP_UNIX  
  /*
   * Set up the plugin function table that Netscape will use to call us.
   */
  if (pluginFuncs->size < sizeof(NPPluginFuncs)) {
    fprintf (stderr, "xine-plugin: plugin function table too small (%d)!\n",
             pluginFuncs->size);
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }

  pluginFuncs->version       = NP_BUILD_VERSION;
  pluginFuncs->size          = sizeof(NPPluginFuncs);
  pluginFuncs->newp          = NewNPP_NewProc(NPP_New);
  pluginFuncs->destroy       = NewNPP_DestroyProc(NPP_Destroy);
  pluginFuncs->setwindow     = NewNPP_SetWindowProc(NPP_SetWindow);
  pluginFuncs->newstream     = NewNPP_NewStreamProc(NPP_NewStream);
  pluginFuncs->destroystream = NewNPP_DestroyStreamProc(NPP_DestroyStream);
  pluginFuncs->asfile        = NewNPP_StreamAsFileProc(NPP_StreamAsFile);
  pluginFuncs->writeready    = NewNPP_WriteReadyProc(NPP_WriteReady);
  pluginFuncs->write         = NewNPP_WriteProc(NPP_Write);
  pluginFuncs->print         = NewNPP_PrintProc(NPP_Print);
  pluginFuncs->urlnotify     = NewNPP_URLNotifyProc(NPP_URLNotify);
  pluginFuncs->event         = NULL;
#ifdef OJI
  pluginFuncs->javaClass     = NULL;
#endif
  /* GetValue might call NPN_CreateObject. */
  if (pFuncs->version >= NP_VERSION_HAS_RUNTIME) {
    pluginFuncs->getvalue    = NewNPP_GetValueProc(NPP_GetValue);
    pluginFuncs->setvalue    = NewNPP_SetValueProc(NPP_SetValue);
  }
#endif

  return NPP_Initialize ();
}

NPError OSCALL
NP_Shutdown (void)
{ 
  NPP_Shutdown ();
  return NPERR_NO_ERROR;
}

