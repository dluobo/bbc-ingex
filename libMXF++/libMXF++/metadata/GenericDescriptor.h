#ifndef __MXFPP_GENERICDESCRIPTOR_H__
#define __MXFPP_GENERICDESCRIPTOR_H__



#include <libMXF++/metadata/base/GenericDescriptorBase.h>


namespace mxfpp
{


class GenericDescriptor : public GenericDescriptorBase
{
public:
    friend class MetadataSetFactory<GenericDescriptor>;

public:
    GenericDescriptor(HeaderMetadata* headerMetadata);
    virtual ~GenericDescriptor();




protected:
    GenericDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
