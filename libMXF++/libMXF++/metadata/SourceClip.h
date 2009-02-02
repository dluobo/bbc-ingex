#ifndef __MXFPP_SOURCECLIP_H__
#define __MXFPP_SOURCECLIP_H__



#include <libMXF++/metadata/base/SourceClipBase.h>


namespace mxfpp
{


class SourceClip : public SourceClipBase
{
public:
    friend class MetadataSetFactory<SourceClip>;

public:
    SourceClip(HeaderMetadata* headerMetadata);
    virtual ~SourceClip();




protected:
    SourceClip(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
