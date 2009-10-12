#include <errno.h>

#include "MXFWriter.h"
#include "MXFWriterException.h"

#include <Logging.h>

using namespace std;
using namespace prodauto;


MXFWriter::MXFWriter()
{
}

MXFWriter::~MXFWriter()
{
}

void MXFWriter::SetCreatingDirectory(string path)
{
    mCreatingDirectory = path;
}

void MXFWriter::SetDestinationDirectory(string path)
{
    mDestinationDirectory = path;
}

void MXFWriter::SetFailureDirectory(string path)
{
    mFailureDirectory = path;
}


