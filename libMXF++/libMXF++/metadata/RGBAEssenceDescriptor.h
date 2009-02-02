#ifndef __MXFPP_RGBAESSENCEDESCRIPTOR_H__
#define __MXFPP_RGBAESSENCEDESCRIPTOR_H__



#include <libMXF++/metadata/base/RGBAEssenceDescriptorBase.h>


namespace mxfpp
{


class RGBAEssenceDescriptor : public RGBAEssenceDescriptorBase
{
public:
    friend class MetadataSetFactory<RGBAEssenceDescriptor>;

public:
    RGBAEssenceDescriptor(HeaderMetadata* headerMetadata);
    virtual ~RGBAEssenceDescriptor();




protected:
    RGBAEssenceDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
