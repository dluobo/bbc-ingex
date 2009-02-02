#ifndef __MXFPP_DMFRAMEWORK_H__
#define __MXFPP_DMFRAMEWORK_H__



#include <libMXF++/metadata/base/DMFrameworkBase.h>


namespace mxfpp
{


class DMFramework : public DMFrameworkBase
{
public:
    friend class MetadataSetFactory<DMFramework>;

public:
    DMFramework(HeaderMetadata* headerMetadata);
    virtual ~DMFramework();




protected:
    DMFramework(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
