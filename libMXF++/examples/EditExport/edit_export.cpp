#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <stdlib.h>

#include <ArchiveMXFReader.h>
#include <DVEncoder.h>
#include <EncoderException.h>
#include <AvidClipWriter.h>



using namespace std;
using namespace mxfpp;


void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <d3 mxf filename> <avid mxf filename prefix>\n", cmd);
}

int main(int argc, const char** argv)
{
    string d3MXFFilename;
    string avidClipPrefix;
    
    if (argc != 3)
    {
        usage(argv[0]);
        exit(1);
    }
    
    d3MXFFilename = argv[1];
    avidClipPrefix = argv[2];
    mxfRational imageAspectRatio = {4, 3};
    
    try
    {
        // create the D3 MXF reader
        ArchiveMXFReader reader(d3MXFFilename);
        
        int64_t duration = reader.getDuration();
        if (duration >= 0)
        {
            printf("Source length is %02d:%02d:%02d:%02d\n",
                (int)(duration / (60 * 60 * 25)),
                (int)((duration % (60 * 60 * 25)) / (60 * 25)),
                (int)(((duration % (60 * 60 * 25)) % (60 * 25)) / 25),
                (int)(((duration % (60 * 60 * 25)) / (60 * 25)) % 25));
        }
        else
        {
            printf("Source length is unknown\n");
        }
        
        // create the DV encoder
        DVEncoder dvEncoder(UYVY_25I_ENCODER_INPUT, DV50_ENCODER_OUTPUT);
        SimpleEncoderOutput encoderOutput;
        
        // create the clip writer
        AvidClipWriter clipWriter(AVID_PAL_25I_PROJECT, imageAspectRatio, false, true);
        clipWriter.setProjectName("Test Project");
        clipWriter.setClipName(avidClipPrefix);
        clipWriter.addUserComment("Descript", "A test file");
        
        // register the D3 tracks
        EssenceParams params;
        
        string videoFilename = avidClipPrefix;
        videoFilename.append("_v1.mxf");
        clipWriter.registerEssenceElement(1, 1, AVID_DVBASED50_ESSENCE, &params, videoFilename);
        
        params.quantizationBits = 16;
        string audio1Filename = avidClipPrefix;
        audio1Filename.append("_a1.mxf");
        clipWriter.registerEssenceElement(2, 1, AVID_PCM_ESSENCE, &params, audio1Filename);
    
        string audio2Filename = avidClipPrefix;
        audio2Filename.append("_a2.mxf");
        clipWriter.registerEssenceElement(3, 2, AVID_PCM_ESSENCE, &params, audio2Filename);
    
        string audio3Filename = avidClipPrefix;
        audio3Filename.append("_a3.mxf");
        clipWriter.registerEssenceElement(4, 3, AVID_PCM_ESSENCE, &params, audio3Filename);
    
        string audio4Filename = avidClipPrefix;
        audio4Filename.append("_a4.mxf");
        clipWriter.registerEssenceElement(5, 4, AVID_PCM_ESSENCE, &params, audio4Filename);
    
        
        // prepare the Avid clip
        clipWriter.prepareToWrite();
        
        while (!reader.isEOF())
        {
            // read a content package
            const ArchiveMXFContentPackage* cp = reader.read();
            
            // encode the video
            dvEncoder.encode(cp->getVideo()->getBytes(), cp->getVideo()->getSize(), &encoderOutput);
            
            // write to the Avid clip
            clipWriter.writeSamples(1, 1, encoderOutput.getBuffer(), encoderOutput.getBufferSize());
            clipWriter.writeSamples(2, 1920, cp->getAudio(0)->getBytes(), cp->getAudio(0)->getSize());
            clipWriter.writeSamples(3, 1920, cp->getAudio(1)->getBytes(), cp->getAudio(1)->getSize());
            clipWriter.writeSamples(4, 1920, cp->getAudio(2)->getBytes(), cp->getAudio(2)->getSize());
            clipWriter.writeSamples(5, 1920, cp->getAudio(3)->getBytes(), cp->getAudio(3)->getSize());
        }

        // complete the writing of the Avid clip
        clipWriter.completeWrite();

    }
    catch (const mxfpp::MXFException& ex)
    {
        fprintf(stderr, "MXF exception thrown: %s\n", ex.getMessage().c_str());
        exit(1);
    }
    catch (const EncoderException& ex)
    {
        fprintf(stderr, "Encoder exception thrown: %s\n", ex.getMessage().c_str());
        exit(1);
    }
    
    return 0;
}

