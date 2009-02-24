/*
 * $Id: HTTPServer.h,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
 *
 * Wraps the shttpd embedded web server
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __INGEX_HTTP_SERVER_H__
#define __INGEX_HTTP_SERVER_H__


#include <shttpd.h>

#include "Threads.h"
#include "JSONObject.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>


namespace ingex
{

typedef struct
{
    std::string name;
    std::string type;
    bool isRequired;
    std::string description;
} HTTPServiceArgument;

typedef struct
{
    std::string code;
    std::string description;
} HTTPServiceExample;

class HTTPServiceDescription
{
public:
    HTTPServiceDescription(std::string uri);
    ~HTTPServiceDescription();

    void setURL(std::string uri);
    void setDescription(std::string description);
    void addArgument(std::string name, std::string type, bool isRequired, std::string description);
    void addExample(std::string code, std::string description);

    std::string getURL();

    friend std::ostringstream& operator<<(std::ostringstream& out, HTTPServiceDescription& descr);

private:
    std::string _url;
    std::string _description;
    std::vector<HTTPServiceArgument> _arguments;
    std::vector<HTTPServiceExample> _examples;
};


class HTTPConnectionState
{
public:
    virtual ~HTTPConnectionState() {}
};


class HTTPServer;

class HTTPConnection
{
public:
    friend void http_service(struct shttpd_arg* arg);
    friend void http_ssi_handler(struct shttpd_arg* arg);

private:
    // used by http_service
    static HTTPConnection* getConnection(struct shttpd_arg* arg);
    static void addClosedConnection(HTTPConnection* connection);

    // connections closed by the client which are deleted in getConnection()
    static std::vector<HTTPConnection*> _closedConnections;

public:
    ~HTTPConnection();


    // state methods

    HTTPConnectionState* getState(std::string id);
    HTTPConnectionState* insertState(std::string id, HTTPConnectionState* state);


    // request methods

    std::string getRequestURI();
    std::string getQueryString();
    std::string getPostData();

    bool haveQueryValue(std::string name);
    std::string getQueryValue(std::string name);

    bool havePostValue(std::string name);
    std::string getPostValue(std::string name);


    // response methods

    std::ostringstream& responseStream();
    void setResponseDataIsReady();
    void setResponseDataIsPartial();

    void startMultipartResponse(std::string boundary);
    void sendJSON(JSONObject* json);
    void sendMultipartJSON(JSONObject* json, std::string boundary);
    void sendOk();
    void sendBadRequest(const char* format, ...);
    void sendServerBusy(const char* format, ...);
    void sendServerError(const char* format, ...);

private:
    HTTPConnection(struct shttpd_arg* arg);

    // used by http_service
    bool requestIsReady();
    void setHaveProcessed();
    bool haveProcessed();
    bool responseIsComplete();
    void copyResponseData();



    struct shttpd_arg* _arg;

    std::map<std::string, HTTPConnectionState*> _stateMap;

    std::string _requestURI;
    std::string _requestMethod;
    std::string _queryString;
    std::string _postData;

    std::ostringstream _buffer;
    bool _responseDataIsReady;
    size_t _partialResponseDataSize;
    bool _responseDataIsPartial;
    size_t _sizeCopied;
    bool _haveProcessed;
    bool _responseIsComplete;
};



class HTTPConnectionHandler
{
public:
    virtual ~HTTPConnectionHandler() {}

    virtual bool processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection) = 0;
};

class HTTPService
{
public:
    HTTPService(HTTPServiceDescription* description_, HTTPConnectionHandler* handler_);
    ~HTTPService();

    HTTPServiceDescription* description;
    HTTPConnectionHandler* handler;
};

class HTTPSSIHandler
{
public:
    virtual ~HTTPSSIHandler() {}

    virtual void processSSIRequest(std::string name, HTTPConnection* connection) = 0;
};

class HTTPSSIHandlerInfo
{
public:
    HTTPSSIHandlerInfo(std::string name_, HTTPSSIHandler* handler_);
    ~HTTPSSIHandlerInfo();

    std::string name;
    HTTPSSIHandler* handler;
};


class HTTPServer : public HTTPConnectionHandler
{
public:
    HTTPServer(int port, std::string documentRoot, std::vector<std::pair<std::string, std::string> > aliases);
    ~HTTPServer();

    // register all handlers before calling start()
    HTTPServiceDescription* registerService(HTTPServiceDescription* serviceDescription,
        HTTPConnectionHandler* handler);

    // register before calling start()
    void registerSSIHandler(std::string name, HTTPSSIHandler* handler);


    void start();


    virtual bool processRequest(HTTPServiceDescription* serviceDescription, HTTPConnection* connection);

private:
    struct shttpd_ctx* _ctx;
    Thread* _serverThread;
    bool _started;
    std::map<std::string, HTTPService*> _services;
    std::map<std::string, HTTPSSIHandlerInfo*> _ssiHandlers;
};



};








#endif
