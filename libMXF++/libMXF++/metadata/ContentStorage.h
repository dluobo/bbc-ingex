#ifndef __MXFPP_CONTENTSTORAGE_H__
#define __MXFPP_CONTENTSTORAGE_H__



#include <libMXF++/metadata/base/ContentStorageBase.h>


namespace mxfpp
{


class ContentStorage : public ContentStorageBase
{
public:
    friend class MetadataSetFactory<ContentStorage>;

public:
    ContentStorage(HeaderMetadata* headerMetadata);
    virtual ~ContentStorage();




protected:
    ContentStorage(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
