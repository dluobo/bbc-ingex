#ifndef __MXFPP_GENERICSOUNDESSENCEDESCRIPTOR_H__
#define __MXFPP_GENERICSOUNDESSENCEDESCRIPTOR_H__



#include <libMXF++/metadata/base/GenericSoundEssenceDescriptorBase.h>


namespace mxfpp
{


class GenericSoundEssenceDescriptor : public GenericSoundEssenceDescriptorBase
{
public:
    friend class MetadataSetFactory<GenericSoundEssenceDescriptor>;

public:
    GenericSoundEssenceDescriptor(HeaderMetadata* headerMetadata);
    virtual ~GenericSoundEssenceDescriptor();




protected:
    GenericSoundEssenceDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
