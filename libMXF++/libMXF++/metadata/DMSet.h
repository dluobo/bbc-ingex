#ifndef __MXFPP_DMSET_H__
#define __MXFPP_DMSET_H__



#include <libMXF++/metadata/base/DMSetBase.h>


namespace mxfpp
{


class DMSet : public DMSetBase
{
public:
    friend class MetadataSetFactory<DMSet>;

public:
    DMSet(HeaderMetadata* headerMetadata);
    virtual ~DMSet();




protected:
    DMSet(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
