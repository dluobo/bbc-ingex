#ifndef __MXFPP_PREFACE_H__
#define __MXFPP_PREFACE_H__



#include <libMXF++/metadata/base/PrefaceBase.h>


namespace mxfpp
{


class Preface : public PrefaceBase
{
public:
    friend class MetadataSetFactory<Preface>;

public:
    Preface(HeaderMetadata* headerMetadata);
    virtual ~Preface();




protected:
    Preface(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
