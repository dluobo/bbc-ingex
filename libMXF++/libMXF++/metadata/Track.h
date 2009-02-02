#ifndef __MXFPP_TRACK_H__
#define __MXFPP_TRACK_H__



#include <libMXF++/metadata/base/TrackBase.h>


namespace mxfpp
{


class Track : public TrackBase
{
public:
    friend class MetadataSetFactory<Track>;

public:
    Track(HeaderMetadata* headerMetadata);
    virtual ~Track();




protected:
    Track(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
