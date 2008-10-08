#ifndef __PRODAUTO_INGEX_PLAYER_LISTENER_H__
#define __PRODAUTO_INGEX_PLAYER_LISTENER_H__


#include <string>
#include <vector>

#include <media_player.h>
#include <x11_common.h>



namespace prodauto
{

class IngexPlayerListener;

class IngexPlayerListenerRegistry
{
public:
    virtual ~IngexPlayerListenerRegistry() {};
    
    /* (un)register listener for player events */
    
    virtual bool registerListener(IngexPlayerListener* listener) = 0;
    virtual bool unregisterListener(IngexPlayerListener* listener) = 0;
};
    
class IngexPlayerListener
{
public:
    IngexPlayerListener(IngexPlayerListenerRegistry* registry)
    : _registry(0)
    {
        registry->registerListener(this);
    }
    virtual ~IngexPlayerListener() 
    {
        if (_registry != 0)
        {
            _registry->unregisterListener(this);
        }
    };
    
    virtual IngexPlayerListenerRegistry* getRegistry()
    {
        return _registry;
    }

    virtual void setRegistry(IngexPlayerListenerRegistry* registry)
    {
        _registry = registry;
    }
    
    virtual void unsetRegistry()
    {
        _registry = 0;
    }
    
    virtual void frameDisplayedEvent(const FrameInfo* frameInfo) = 0;
    virtual void frameDroppedEvent(const FrameInfo* lastFrameInfo) = 0;
    virtual void stateChangeEvent(const MediaPlayerStateEvent* event) = 0;
    virtual void endOfSourceEvent(const FrameInfo* lastReadFrameInfo) = 0;
    virtual void startOfSourceEvent(const FrameInfo* firstReadFrameInfo) = 0;
    virtual void playerCloseRequested() = 0;
    virtual void playerClosed() = 0;
    virtual void keyPressed(int key) = 0;
    virtual void keyReleased(int key) = 0;
    virtual void progressBarPositionSet(float position) = 0;
    virtual void mouseClicked(int imageWidth, int imageHeight, int xPos, int yPos) = 0;
    
    
protected:
    IngexPlayerListenerRegistry* _registry;
};


};



#endif



