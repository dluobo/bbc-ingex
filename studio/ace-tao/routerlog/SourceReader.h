// SourceReader.h: interface for the SourceReader class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(SourceReader_h)
#define SourceReader_h


#include <Database.h>
#include <string>

class SourceReader  
{
public:
    SourceReader();
    virtual ~SourceReader();

    bool Init(const std::string & name, const std::string & dbhost, const std::string & dbname, const std::string & dbuser, const std::string & dbpw);
    std::string SourceName(int value);
    bool GetSource(int index, std::string & name, uint32_t & track);
    bool GetDestination(int index, std::string & name);

private:
    prodauto::Database * mpDb;
    prodauto::RouterConfig * mpRc;
};

#endif //#if !defined(SourceReader_h)
