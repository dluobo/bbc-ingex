#ifndef __MXFPP_SOURCEPACKAGE_H__
#define __MXFPP_SOURCEPACKAGE_H__



#include <libMXF++/metadata/base/SourcePackageBase.h>


namespace mxfpp
{


class SourcePackage : public SourcePackageBase
{
public:
    friend class MetadataSetFactory<SourcePackage>;

public:
    SourcePackage(HeaderMetadata* headerMetadata);
    virtual ~SourcePackage();




protected:
    SourcePackage(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
