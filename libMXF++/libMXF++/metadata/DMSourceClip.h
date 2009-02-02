#ifndef __MXFPP_DMSOURCECLIP_H__
#define __MXFPP_DMSOURCECLIP_H__



#include <libMXF++/metadata/base/DMSourceClipBase.h>


namespace mxfpp
{


class DMSourceClip : public DMSourceClipBase
{
public:
    friend class MetadataSetFactory<DMSourceClip>;

public:
    DMSourceClip(HeaderMetadata* headerMetadata);
    virtual ~DMSourceClip();




protected:
    DMSourceClip(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
