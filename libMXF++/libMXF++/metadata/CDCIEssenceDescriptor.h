#ifndef __MXFPP_CDCIESSENCEDESCRIPTOR_H__
#define __MXFPP_CDCIESSENCEDESCRIPTOR_H__



#include <libMXF++/metadata/base/CDCIEssenceDescriptorBase.h>


namespace mxfpp
{


class CDCIEssenceDescriptor : public CDCIEssenceDescriptorBase
{
public:
    friend class MetadataSetFactory<CDCIEssenceDescriptor>;

public:
    CDCIEssenceDescriptor(HeaderMetadata* headerMetadata);
    virtual ~CDCIEssenceDescriptor();




protected:
    CDCIEssenceDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
