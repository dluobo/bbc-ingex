#ifndef __MXFPP_STATICTRACK_H__
#define __MXFPP_STATICTRACK_H__



#include <libMXF++/metadata/base/StaticTrackBase.h>


namespace mxfpp
{


class StaticTrack : public StaticTrackBase
{
public:
    friend class MetadataSetFactory<StaticTrack>;

public:
    StaticTrack(HeaderMetadata* headerMetadata);
    virtual ~StaticTrack();




protected:
    StaticTrack(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
