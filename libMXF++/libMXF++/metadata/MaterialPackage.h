#ifndef __MXFPP_MATERIALPACKAGE_H__
#define __MXFPP_MATERIALPACKAGE_H__



#include <libMXF++/metadata/base/MaterialPackageBase.h>


namespace mxfpp
{


class MaterialPackage : public MaterialPackageBase
{
public:
    friend class MetadataSetFactory<MaterialPackage>;

public:
    MaterialPackage(HeaderMetadata* headerMetadata);
    virtual ~MaterialPackage();




protected:
    MaterialPackage(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
