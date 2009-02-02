#ifndef __MXFPP_NETWORKLOCATOR_H__
#define __MXFPP_NETWORKLOCATOR_H__



#include <libMXF++/metadata/base/NetworkLocatorBase.h>


namespace mxfpp
{


class NetworkLocator : public NetworkLocatorBase
{
public:
    friend class MetadataSetFactory<NetworkLocator>;

public:
    NetworkLocator(HeaderMetadata* headerMetadata);
    virtual ~NetworkLocator();




protected:
    NetworkLocator(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
