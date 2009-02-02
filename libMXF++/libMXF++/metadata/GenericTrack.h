#ifndef __MXFPP_GENERICTRACK_H__
#define __MXFPP_GENERICTRACK_H__



#include <libMXF++/metadata/base/GenericTrackBase.h>


namespace mxfpp
{


class GenericTrack : public GenericTrackBase
{
public:
    friend class MetadataSetFactory<GenericTrack>;

public:
    GenericTrack(HeaderMetadata* headerMetadata);
    virtual ~GenericTrack();




protected:
    GenericTrack(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
