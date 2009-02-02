#ifndef __MXFPP_WAVEAUDIODESCRIPTOR_H__
#define __MXFPP_WAVEAUDIODESCRIPTOR_H__



#include <libMXF++/metadata/base/WaveAudioDescriptorBase.h>


namespace mxfpp
{


class WaveAudioDescriptor : public WaveAudioDescriptorBase
{
public:
    friend class MetadataSetFactory<WaveAudioDescriptor>;

public:
    WaveAudioDescriptor(HeaderMetadata* headerMetadata);
    virtual ~WaveAudioDescriptor();




protected:
    WaveAudioDescriptor(HeaderMetadata* headerMetadata, ::MXFMetadataSet* cMetadataSet);
};


};


#endif
