#ifndef __MXFPP_TEXTLOCATOR_H__
#define __MXFPP_TEXTLOCATOR_H__



#include <libMXF++/metadata/base/TextLocatorBase.h>


namespace mxfpp
{


class TextLocator : public TextLocatorBase
{
public:
    friend class MetadataSetFactory<TextLocator>;

public:
    TextLocator(HeaderMetadata* headerMetadata);
    virtual ~TextLocator();




protected:
    TextLocator(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
