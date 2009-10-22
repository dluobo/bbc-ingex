#ifndef __PRODAUTO_MXF_WRITER_H__
#define __PRODAUTO_MXF_WRITER_H__

#include <string>
#include <vector>

#include <Package.h>
#include <PackageGroup.h>


namespace prodauto
{

class MXFWriter
{
public:
    MXFWriter();
    virtual ~MXFWriter();
    
    // configure
    void SetCreatingDirectory(std::string path);
    void SetDestinationDirectory(std::string path);
    void SetFailureDirectory(std::string path);
    
public:
    // prepare the writer
    virtual void PrepareToWrite(PackageGroup *package_group, bool take_ownership = false) = 0;


    // write essence data samples
    virtual void WriteSamples(uint32_t mp_track_id, uint32_t num_samples, const uint8_t *data, uint32_t data_size) = 0;
    virtual void StartSampleData(uint32_t mp_track_id) = 0;
    virtual void WriteSampleData(uint32_t mp_track_id, const uint8_t *data, uint32_t data_size) = 0;
    virtual void EndSampleData(uint32_t mp_track_id, uint32_t num_samples) = 0;
    

    // complete or abort
    virtual void CompleteWriting(bool save_to_database) = 0;
    virtual void AbortWriting(bool delete_files) = 0;

protected:
    std::string mCreatingDirectory;
    std::string mDestinationDirectory;
    std::string mFailureDirectory;
};


};


#endif

