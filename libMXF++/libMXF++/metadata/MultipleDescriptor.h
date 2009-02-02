#ifndef __MXFPP_MULTIPLEDESCRIPTOR_H__
#define __MXFPP_MULTIPLEDESCRIPTOR_H__



#include <libMXF++/metadata/base/MultipleDescriptorBase.h>


namespace mxfpp
{


class MultipleDescriptor : public MultipleDescriptorBase
{
public:
    friend class MetadataSetFactory<MultipleDescriptor>;

public:
    MultipleDescriptor(HeaderMetadata* headerMetadata);
    virtual ~MultipleDescriptor();




protected:
    MultipleDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
