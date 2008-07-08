/*
 * $Id: HTTPServer.cpp,v 1.1 2008/07/08 16:24:40 philipn Exp $
 *
 * Wraps the shttpd embedded web server
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
/*
    Services matching URL's are registered in the HTTPServer. 
    
    The service information set includes descriptive information which can be 
    viewed at /services.html.
    
    The HTTPConnectionHandler provided by the service is called when the 
    corresponding URL is requested. The handler is passed a HTTPConnection 
    object which manages the request and reponse data for the connection.
    
    Server side includes handlers, HTTPSSIHandler, can be registered in the
    HTTPServer and are called when data is required for a SSI HTML page
*/
 
#include <string.h>
#include <sstream>

#include "HTTPServer.h"
#include "Logging.h"
#include "RecorderException.h"


using namespace std;
using namespace rec;


// large enough to contain the our http request and responses in one buffer  
#define HTTP_IO_BUFFER_SIZE_STR          "16384"


static const char* g_servicesURL = "/services.html";
static const char* g_servicesURLDescription = "Lists the dynamic services exported by the Ingex Recorder";


namespace rec
{
    
// this function is called by the shttpd library
void http_service(struct shttpd_arg* arg)
{
    HTTPService* service = (HTTPService*)arg->user_data;
    HTTPConnection* connection = HTTPConnection::getConnection(arg);
    
    // handle broken connections
    if (arg->flags & SHTTPD_CONNECTION_ERROR) 
    {
        Logging::info("HTTP connection was broken\n");
        HTTPConnection::addClosedConnection(connection);
        arg->state = 0;
        return;
    }
    
    // handle post data buffer full error (this shouldn't happen for valid requests)
    if (arg->flags & SHTTPD_POST_BUFFER_FULL)
    {
        Logging::warning("HTTP post buffer is full\n");
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        delete connection;
        arg->state = 0;
        return;
    }
    
    // only continue when all post data has been received
    if (!connection->requestIsReady())
    {
        return;
    }
    
    // signal end of output and cleanup when the response is complete 
    if (connection->responseIsComplete())
    {
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        delete connection;
        arg->state = 0;
        return;
    }

    // handle the connection if not already processed
    if (!connection->haveProcessed())
    {
        service->handler->processRequest(service->description, connection);
        connection->setHaveProcessed();
    }
    
    // copy response data to the remote client
    connection->copyResponseData();
}


// this function is called by the shttpd library
void http_ssi_handler(struct shttpd_arg* arg)
{
    HTTPSSIHandlerInfo* handlerInfo = (HTTPSSIHandlerInfo*)arg->user_data;
    HTTPConnection* connection = HTTPConnection::getConnection(arg);

    // process the request
    handlerInfo->handler->processSSIRequest(handlerInfo->name, connection);

    // copy response data to the remote client
    connection->copyResponseData();
    
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    delete connection;
    arg->state = 0;
}


};




class HTTPServerThreadWorker : public ThreadWorker
{
public:
    HTTPServerThreadWorker(struct shttpd_ctx* ctx)
    : _ctx(ctx), _hasStopped(false), _stop(false)
    {}
    virtual ~HTTPServerThreadWorker()
    {
        shttpd_fini(_ctx);
    }
    
    virtual void start()
    {
        GUARD_THREAD_START(_hasStopped);
        
        _stop = false;
        
        while (!_stop)
        {
            shttpd_poll(_ctx, 1000);
        }
    }
    
    virtual void stop()
    {
        _stop = true;
    }
    
    virtual bool hasStopped() const
    {
        return _hasStopped;
    }
    
private:
    struct shttpd_ctx* _ctx;
    bool _hasStopped;
    bool _stop;
};




HTTPServiceDescription::HTTPServiceDescription(string url)
: _url(url)
{
}

HTTPServiceDescription::~HTTPServiceDescription()
{
}

void HTTPServiceDescription::setURL(string url)
{
    _url = url;
}

void HTTPServiceDescription::setDescription(string description)
{
    _description = description;
}

void HTTPServiceDescription::addArgument(string name, string type, bool isRequired, string description)
{
    HTTPServiceArgument arg;
    arg.name = name;
    arg.type = type;
    arg.isRequired = isRequired;
    arg.description = description;
    
    _arguments.push_back(arg);
}

void HTTPServiceDescription::addExample(string code, string description)
{
    HTTPServiceExample example;
    example.code = code;
    example.description = description;
    
    _examples.push_back(example);
}

string HTTPServiceDescription::getURL()
{
    return _url;
}


namespace rec
{
    
ostringstream& operator<<(ostringstream& out, HTTPServiceDescription& descr)
{
    out << "<p class=\"service-descr\">" << endl;
    out << "<em>" << descr._description << "</em>" << endl;
    out << "</p>" << endl;
        
    if (descr._arguments.size() != 0)
    {
        out << "<p class=\"service-args\">" << endl;
        out << "Arguments:" << endl;
        
        vector<HTTPServiceArgument>::const_iterator iter;
        for (iter = descr._arguments.begin(); iter != descr._arguments.end(); iter++)
        {
            if (iter != descr._arguments.begin())
            {
                out << "<br/>" << endl;
            }
            out << "<div class=\"service-arg\">" << endl;
            out << "<b>" << (*iter).name << "</b>: " << (*iter).type << " ";
            out << (((*iter).isRequired) ? "(required) " : "(optional) ") << "<br/>" << endl;
            out << "<em>" << (*iter).description << "</em>" << endl;
            out << "</div>" << endl;
        }
        
        out << "</p>" << endl;
    }

    if (descr._examples.size() != 0)
    {
        out << "<p class=\"service-examples\">" << endl;
        out << "Examples:" << endl;
        
        vector<HTTPServiceExample>::const_iterator iter;
        for (iter = descr._examples.begin(); iter != descr._examples.end(); iter++)
        {
            if (iter != descr._examples.begin())
            {
                out << "<br/>" << endl;
            }
            out << "<div class=\"service-example\">" << endl;
            out << "<code>" << (*iter).code << "</code>:<br/>" << endl;
            out << "<em>" << (*iter).description << "</em>" << endl;
            out << "</div>" << endl;
        }
        out << "</p>" << endl;
    }
    
    return out;
}

};



// static member
vector<HTTPConnection*> HTTPConnection::_closedConnections;


HTTPConnection* HTTPConnection::getConnection(struct shttpd_arg* arg)
{
    HTTPConnection* ret;
    
    // cleanup closed connections if they are no longer in use
    vector<HTTPConnection*>::iterator iter = _closedConnections.begin();
    while (iter != _closedConnections.end())
    {
        if (!(*iter)->haveProcessed() || (*iter)->responseIsComplete())
        {
            // connection is not in use - delete it
            delete *iter;
            iter = _closedConnections.erase(iter);
        }
        else
        {
            iter++;
        }
    }
    
    // get or create connection object
    if (arg->state == 0)
    {
        ret = new HTTPConnection(arg);
    }
    else
    {
        ret = (HTTPConnection*)arg->state;
        ret->_arg = arg;
    }
    
    return ret;
}

void HTTPConnection::addClosedConnection(HTTPConnection* connection)
{
    _closedConnections.push_back(connection);
}


HTTPConnection::HTTPConnection(struct shttpd_arg* arg)
: _arg(arg), _responseDataIsReady(false), _sizeCopied(0), _haveProcessed(false), _responseIsComplete(false)
{
    _arg->state = this;
    
    const char* ru = shttpd_get_env(_arg, "REQUEST_URI");
    _requestURI = (ru == NULL) ? "" : ru; 
    
    const char* rm = shttpd_get_env(_arg, "REQUEST_METHOD");
    _requestMethod = (rm == NULL) ? "" : rm; 

    const char* qs = shttpd_get_env(_arg, "QUERY_STRING");
    _queryString = (qs == NULL) ? "" : qs; 
}

HTTPConnection::~HTTPConnection()
{
}

string HTTPConnection::getRequestURI()
{
    return _requestURI;
}

string HTTPConnection::getQueryString()
{
    return _queryString;
}

string HTTPConnection::getPostData()
{
    return _postData;
}

bool HTTPConnection::haveQueryValue(string name)
{
    char value[1];

    return _queryString.size() > 0 && 
        shttpd_get_var(name.c_str(), _queryString.c_str(), _queryString.size(), 
            value, sizeof(value)) >= 0;
}

string HTTPConnection::getQueryValue(string name)
{
    // TODO: avoid limiting value size
    char value[256];
    value[0] = '\0';

    if (_queryString.size() > 0)
    {
        shttpd_get_var(name.c_str(), _queryString.c_str(), _queryString.size(), 
            value, sizeof(value));
        value[sizeof(value) - 1] = '\0';
    }
    
    return value;
}

bool HTTPConnection::havePostValue(string name)
{
    char value[1];

    return _postData.size() > 0 &&
        shttpd_get_var(name.c_str(), _postData.c_str(), (int)_postData.size(), 
            value, sizeof(value)) >= 0;
}

string HTTPConnection::getPostValue(string name)
{
    // TODO: avoid limiting value size
    char value[4096];
    value[0] = '\0';

    shttpd_get_var(name.c_str(), _postData.c_str(), (int)_postData.size(), 
        value, sizeof(value));

    return value;
}

ostringstream& HTTPConnection::responseStream()
{
    return _buffer;
}

void HTTPConnection::setResponseDataIsReady()
{
    _responseDataIsReady = true;
}

void HTTPConnection::sendJSON(JSONObject* json)
{
    // NOTE: we don't use mime type 'application/json' because it is easier
    // to debug in the browser if it is 'text/plain' because the browser 
    // is then able to display it directly rather than prompting the user to 
    // save it to disk
    _buffer << "HTTP/1.1 200 OK\r\n";
    _buffer << "Content-type: text/plain\r\n\r\n";
    _buffer << json->toString();
    setResponseDataIsReady();
}

void HTTPConnection::sendOk()
{
    _buffer << "HTTP/1.1 200 OK\r\n\r\n";
    setResponseDataIsReady();
}

void HTTPConnection::sendBadRequest(string description)
{
    _buffer << "HTTP/1.1 400 " << description << "\r\n\r\n";
    setResponseDataIsReady();
}

void HTTPConnection::sendServerBusy(string description)
{
    _buffer << "HTTP/1.1 503 " << description << "\r\n\r\n";
    setResponseDataIsReady();
}

void HTTPConnection::sendServerError(string description)
{
    _buffer << "HTTP/1.1 500 " << description << "\r\n\r\n";
    setResponseDataIsReady();
}
    
bool HTTPConnection::requestIsReady()
{
    // TODO: check for closed connection
    
    // append POST data
    if (_requestMethod.compare("POST") == 0)
    {
        if (_arg->in.num_bytes < _arg->in.len)
        {
            _postData.append(_arg->in.buf + _arg->in.num_bytes, _arg->in.len - _arg->in.num_bytes);

            // Tell shttpd we have processed the data
            _arg->in.num_bytes = _arg->in.len;
        }
    }
    
    return !(_arg->flags & SHTTPD_MORE_POST_DATA);
}

void HTTPConnection::setHaveProcessed()
{
    _haveProcessed = true;
}

bool HTTPConnection::haveProcessed()
{
    return _haveProcessed;
}

bool HTTPConnection::responseIsComplete()
{
    return _responseIsComplete;
}

void HTTPConnection::copyResponseData()
{
    if (!_responseDataIsReady || // only copy data when it is ready 
        _responseIsComplete) // data was already copied
    {
        return;
    }
    
    size_t bufferSize = _buffer.str().size();
    size_t copySize = _arg->out.len - _arg->out.num_bytes;
    if (copySize > bufferSize - _sizeCopied)
    {
        copySize = bufferSize - _sizeCopied;
    }
    
    if (copySize > 0)
    {
        memcpy(_arg->out.buf + _arg->out.num_bytes, &_buffer.str().c_str()[_sizeCopied], copySize);
        _arg->out.num_bytes += copySize;
        _sizeCopied += copySize;
    }

    if (_sizeCopied >= bufferSize)
    {
        _responseIsComplete = true;
    }
}



HTTPService::HTTPService(HTTPServiceDescription* description_, HTTPConnectionHandler* handler_)
: description(description_), handler(handler_)
{
}

HTTPService::~HTTPService()
{
    delete description;
    // handler is not owned
}


HTTPSSIHandlerInfo::HTTPSSIHandlerInfo(std::string name_, HTTPSSIHandler* handler_)
: name(name_), handler(handler_)
{
}

HTTPSSIHandlerInfo::~HTTPSSIHandlerInfo()
{
    // handler is not owned
}



HTTPServer::HTTPServer(int port, string documentRoot, vector<pair<string, string> > aliases)
: _ctx(0), _serverThread(0), _started(false)
{
    if (aliases.empty())
    {
        REC_CHECK((_ctx = shttpd_init(NULL, 
            "document_root", documentRoot.c_str(),
            "io_buf_size", HTTP_IO_BUFFER_SIZE_STR,
            NULL)) != NULL);
    }
    else
    {
        string aliasString;
        vector<pair<string, string> >::const_iterator iter;
        for (iter = aliases.begin(); iter != aliases.end(); iter++)
        {
            if (iter != aliases.begin())
            {
                aliasString += ",";
            }
            aliasString += (*iter).first + "=" + (*iter).second;
        }
        REC_CHECK((_ctx = shttpd_init(NULL, 
            "document_root", documentRoot.c_str(),
            "io_buf_size", HTTP_IO_BUFFER_SIZE_STR,
            "aliases", aliasString.c_str(),
            NULL)) != NULL);
    }
    REC_CHECK(shttpd_listen(_ctx, port, 0) >= 0);
    
    
    HTTPServiceDescription* serviceDesc;
    serviceDesc = registerService(new HTTPServiceDescription(g_servicesURL), this);
    serviceDesc->setDescription(g_servicesURLDescription);

    _serverThread = new Thread(new HTTPServerThreadWorker(_ctx), true);

    Logging::info("HTTP server is listening on port %d\n", port);
}

HTTPServer::~HTTPServer()
{
    delete _serverThread;
    
    map<string, HTTPService*>::const_iterator iter1;
    for (iter1 = _services.begin(); iter1 != _services.end(); iter1++)
    {
        delete (*iter1).second;
    }

    map<string, HTTPSSIHandlerInfo*>::const_iterator iter2;
    for (iter2 = _ssiHandlers.begin(); iter2 != _ssiHandlers.end(); iter2++)
    {
        delete (*iter2).second;
    }
}

HTTPServiceDescription* HTTPServer::registerService(HTTPServiceDescription* serviceDescription, 
    HTTPConnectionHandler* handler)
{
    REC_ASSERT(!_started);
    
    pair<map<string, HTTPService*>::iterator, bool> result = 
        _services.insert(pair<string, HTTPService*>(
            serviceDescription->getURL(), new HTTPService(serviceDescription, handler)));
    if (!result.second)
    {
        REC_LOGTHROW(("Attempting to register a HTTP service ('%s') that is already registered", 
            serviceDescription->getURL().c_str()));
    }
        
    shttpd_register_uri(_ctx, serviceDescription->getURL().c_str(), http_service, (*result.first).second);
    
    return (*result.first).second->description;
}

void HTTPServer::registerSSIHandler(string name, HTTPSSIHandler* handler)
{
    REC_ASSERT(!_started);
    
    pair<map<string, HTTPSSIHandlerInfo*>::iterator, bool> result = 
        _ssiHandlers.insert(pair<string, HTTPSSIHandlerInfo*>(
            name, new HTTPSSIHandlerInfo(name, handler)));
    if (!result.second)
    {
        REC_LOGTHROW(("Attempting to register a HTTP SSI handler ('%s') that is already registered", 
            name.c_str()));
    }
    
    shttpd_register_ssi_func(_ctx, name.c_str(), http_ssi_handler, (*result.first).second);
}

void HTTPServer::start()
{
    REC_ASSERT(!_started);
    
    _serverThread->start();
    _started = true;
}

void HTTPServer::processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection)
{
    const char* pageTop = 
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
        "<html>\n"
        "<head>\n"
        "<title>Ingex Services</title>\n"
        "<meta HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=iso-8859-1\"/>\n"
        "<link href=\"/styles/main.css\" rel=\"stylesheet\" rev=\"stylesheet\" type=\"text/css\" media=\"screen\"/>\n"
        "</head>\n"
        "<body\n"
        "<h1>Ingex Recorder Services</h1>\n";
    const char* pageBottom = 
        "</body>\n"
        "</html>\n";

    
    connection->responseStream() << "HTTP/1.1 200 OK\r\n";
    connection->responseStream() << "Content-Type: text/html\r\n\r\n";
    
    connection->responseStream() << pageTop;
    
    map<string, HTTPService*>::const_iterator iter;
    for (iter = _services.begin(); iter != _services.end(); iter++)
    {
        connection->responseStream() << "<a href=\"" << (*iter).second->description->getURL() << 
            "\">" << (*iter).second->description->getURL() << "</a><br/>" << endl;
        connection->responseStream() << *((*iter).second->description) << "<br/>" << endl;
    }
    
    connection->responseStream() << pageBottom;
    connection->setResponseDataIsReady();
}

