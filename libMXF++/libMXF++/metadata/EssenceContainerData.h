#ifndef __MXFPP_ESSENCECONTAINERDATA_H__
#define __MXFPP_ESSENCECONTAINERDATA_H__



#include <libMXF++/metadata/base/EssenceContainerDataBase.h>


namespace mxfpp
{


class EssenceContainerData : public EssenceContainerDataBase
{
public:
    friend class MetadataSetFactory<EssenceContainerData>;

public:
    EssenceContainerData(HeaderMetadata* headerMetadata);
    virtual ~EssenceContainerData();




protected:
    EssenceContainerData(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
