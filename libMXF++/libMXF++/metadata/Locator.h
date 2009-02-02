#ifndef __MXFPP_LOCATOR_H__
#define __MXFPP_LOCATOR_H__



#include <libMXF++/metadata/base/LocatorBase.h>


namespace mxfpp
{


class Locator : public LocatorBase
{
public:
    friend class MetadataSetFactory<Locator>;

public:
    Locator(HeaderMetadata* headerMetadata);
    virtual ~Locator();




protected:
    Locator(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
