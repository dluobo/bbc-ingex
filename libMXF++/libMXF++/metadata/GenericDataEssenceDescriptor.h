#ifndef __MXFPP_GENERICDATAESSENCEDESCRIPTOR_H__
#define __MXFPP_GENERICDATAESSENCEDESCRIPTOR_H__



#include <libMXF++/metadata/base/GenericDataEssenceDescriptorBase.h>


namespace mxfpp
{


class GenericDataEssenceDescriptor : public GenericDataEssenceDescriptorBase
{
public:
    friend class MetadataSetFactory<GenericDataEssenceDescriptor>;

public:
    GenericDataEssenceDescriptor(HeaderMetadata* headerMetadata);
    virtual ~GenericDataEssenceDescriptor();




protected:
    GenericDataEssenceDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
