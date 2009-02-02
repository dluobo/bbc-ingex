#ifndef __MXFPP_GENERICPACKAGE_H__
#define __MXFPP_GENERICPACKAGE_H__



#include <libMXF++/metadata/base/GenericPackageBase.h>


namespace mxfpp
{


class GenericPackage : public GenericPackageBase
{
public:
    friend class MetadataSetFactory<GenericPackage>;

public:
    GenericPackage(HeaderMetadata* headerMetadata);
    virtual ~GenericPackage();




protected:
    GenericPackage(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
