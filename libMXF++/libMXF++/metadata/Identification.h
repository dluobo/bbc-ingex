#ifndef __MXFPP_IDENTIFICATION_H__
#define __MXFPP_IDENTIFICATION_H__



#include <libMXF++/metadata/base/IdentificationBase.h>


namespace mxfpp
{


class Identification : public IdentificationBase
{
public:
    friend class MetadataSetFactory<Identification>;

public:
    Identification(HeaderMetadata* headerMetadata);
    virtual ~Identification();

    void initialise(std::string companyName, std::string productName, std::string versionString, mxfUUID productUID);    
    

protected:
    Identification(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
