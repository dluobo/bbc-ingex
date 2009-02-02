#ifndef __MXFPP_EVENTTRACK_H__
#define __MXFPP_EVENTTRACK_H__



#include <libMXF++/metadata/base/EventTrackBase.h>


namespace mxfpp
{


class EventTrack : public EventTrackBase
{
public:
    friend class MetadataSetFactory<EventTrack>;

public:
    EventTrack(HeaderMetadata* headerMetadata);
    virtual ~EventTrack();




protected:
    EventTrack(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
