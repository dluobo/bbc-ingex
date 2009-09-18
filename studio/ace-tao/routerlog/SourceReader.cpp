// SourceReader.cpp: implementation of the SourceReader class.
// 
// substitutes the integer value of the router source for a string
//
//////////////////////////////////////////////////////////////////////

#include "SourceReader.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SourceReader::SourceReader()
: mpDb(0), mpRc(0)
{
}

SourceReader::~SourceReader()
{
}

bool SourceReader::Init(const std::string & name, const std::string & dbhost, const std::string & dbname, const std::string & dbuser, const std::string & dbpw)
{
    // Init database
    try
    {
        prodauto::Database::initialise(dbhost.c_str(), dbname.c_str(), dbuser.c_str(), dbpw.c_str(), 2, 8);
        mpDb = prodauto::Database::getInstance();
    }
    catch (...)
    {
        mpDb = 0;
    }

    if (mpDb)
    {
        try
        {
            mpRc = mpDb->loadRouterConfig(name.c_str());
        }
        catch (...)
        {
            mpRc = 0;
        }
    }

    return (mpDb != 0);
}

#if 0
std::string SourceReader::SourceName(int index)
{
    std::string name;

    if (index == 1) name = "Camera 1";
    else if (index == 2) name = "Camera 2";
    else if (index == 3) name = "Camera 3";
    else if (index == 4) name = "Camera 4";
    else if (index == 5) name = "Camera 5";
    else if (index == 6) name = "Camera 6";
    else if (index == 7) name = "Camera 7";
    else if (index == 8) name = "Camera 8";
    else if (index == 9) name = "Camera 9";
    else if (index == 10) name = "Camera 10";
    else if (index == 11) name = "Camera 11";
    else if (index == 12) name = "Camera 12";
    else if (index == 13) name = "Camera 13";
    else if (index == 14) name = "Camera 14";
    else if (index == 15) name = "Camera 15";
    else if (index == 16) name = "Camera 16";

    else name = "out of range";

    return name;
}
#endif

bool SourceReader::GetSource(int index, std::string & name, uint32_t & track)
{
    prodauto::SourceConfig * sc = 0;

    if (mpRc)
    {
        mpRc->getInputSourceConfig(index, &sc, &track);
    }

    if (sc)
    {
        name = sc->name;
    }
    else
    {
        name = "unknown";
        track = 0;
    }

    return (sc != 0);
}

bool SourceReader::GetDestination(int index, std::string & name)
{
    prodauto::RouterOutputConfig * roc = 0;

    if (mpRc)
    {
        roc = mpRc->getOutputConfig(index);
    }

    if (roc)
    {
        name = roc->name;
    }
    else
    {
        name = "unknown";
    }

    return (roc != 0);
}

