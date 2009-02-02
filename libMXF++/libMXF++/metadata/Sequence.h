#ifndef __MXFPP_SEQUENCE_H__
#define __MXFPP_SEQUENCE_H__



#include <libMXF++/metadata/base/SequenceBase.h>


namespace mxfpp
{


class Sequence : public SequenceBase
{
public:
    friend class MetadataSetFactory<Sequence>;

public:
    Sequence(HeaderMetadata* headerMetadata);
    virtual ~Sequence();




protected:
    Sequence(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
