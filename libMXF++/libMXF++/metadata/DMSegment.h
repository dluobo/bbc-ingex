#ifndef __MXFPP_DMSEGMENT_H__
#define __MXFPP_DMSEGMENT_H__



#include <libMXF++/metadata/base/DMSegmentBase.h>


namespace mxfpp
{


class DMSegment : public DMSegmentBase
{
public:
    friend class MetadataSetFactory<DMSegment>;

public:
    DMSegment(HeaderMetadata* headerMetadata);
    virtual ~DMSegment();




protected:
    DMSegment(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
