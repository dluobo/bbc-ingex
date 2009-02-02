#ifndef __MXFPP_TIMECODECOMPONENT_H__
#define __MXFPP_TIMECODECOMPONENT_H__



#include <libMXF++/metadata/base/TimecodeComponentBase.h>


namespace mxfpp
{


class TimecodeComponent : public TimecodeComponentBase
{
public:
    friend class MetadataSetFactory<TimecodeComponent>;

public:
    TimecodeComponent(HeaderMetadata* headerMetadata);
    virtual ~TimecodeComponent();




protected:
    TimecodeComponent(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
