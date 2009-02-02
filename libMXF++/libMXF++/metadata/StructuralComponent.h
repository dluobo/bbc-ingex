#ifndef __MXFPP_STRUCTURALCOMPONENT_H__
#define __MXFPP_STRUCTURALCOMPONENT_H__



#include <libMXF++/metadata/base/StructuralComponentBase.h>


namespace mxfpp
{


class StructuralComponent : public StructuralComponentBase
{
public:
    friend class MetadataSetFactory<StructuralComponent>;

public:
    StructuralComponent(HeaderMetadata* headerMetadata);
    virtual ~StructuralComponent();




protected:
    StructuralComponent(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
