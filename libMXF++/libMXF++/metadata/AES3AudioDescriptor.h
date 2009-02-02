#ifndef __MXFPP_AES3AUDIODESCRIPTOR_H__
#define __MXFPP_AES3AUDIODESCRIPTOR_H__



#include <libMXF++/metadata/base/AES3AudioDescriptorBase.h>


namespace mxfpp
{


class AES3AudioDescriptor : public AES3AudioDescriptorBase
{
public:
    friend class MetadataSetFactory<AES3AudioDescriptor>;

public:
    AES3AudioDescriptor(HeaderMetadata* headerMetadata);
    virtual ~AES3AudioDescriptor();




protected:
    AES3AudioDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
