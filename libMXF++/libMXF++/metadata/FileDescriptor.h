#ifndef __MXFPP_FILEDESCRIPTOR_H__
#define __MXFPP_FILEDESCRIPTOR_H__



#include <libMXF++/metadata/base/FileDescriptorBase.h>


namespace mxfpp
{


class FileDescriptor : public FileDescriptorBase
{
public:
    friend class MetadataSetFactory<FileDescriptor>;

public:
    FileDescriptor(HeaderMetadata* headerMetadata);
    virtual ~FileDescriptor();




protected:
    FileDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
