#ifndef __MXFPP_INTERCHANGEOBJECT_H__
#define __MXFPP_INTERCHANGEOBJECT_H__



#include <libMXF++/metadata/base/InterchangeObjectBase.h>


namespace mxfpp
{


class InterchangeObject : public InterchangeObjectBase
{
public:
    friend class MetadataSetFactory<InterchangeObject>;

public:
    InterchangeObject(HeaderMetadata* headerMetadata);
    virtual ~InterchangeObject();




protected:
    InterchangeObject(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
