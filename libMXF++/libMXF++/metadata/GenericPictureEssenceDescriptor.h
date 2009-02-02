#ifndef __MXFPP_GENERICPICTUREESSENCEDESCRIPTOR_H__
#define __MXFPP_GENERICPICTUREESSENCEDESCRIPTOR_H__



#include <libMXF++/metadata/base/GenericPictureEssenceDescriptorBase.h>


namespace mxfpp
{


class GenericPictureEssenceDescriptor : public GenericPictureEssenceDescriptorBase
{
public:
    friend class MetadataSetFactory<GenericPictureEssenceDescriptor>;

public:
    GenericPictureEssenceDescriptor(HeaderMetadata* headerMetadata);
    virtual ~GenericPictureEssenceDescriptor();




protected:
    GenericPictureEssenceDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
