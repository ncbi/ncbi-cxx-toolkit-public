#ifndef NETCACHE_REQUEST_HANDLER__HPP
#define NETCACHE_REQUEST_HANDLER__HPP
/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include "netcached.hpp"

BEGIN_NCBI_SCOPE

const size_t kNetworkBufferSize = 64 * 1024;

// NetCache request handler host
class CNetCache_RequestHandlerHost
{
public:
    virtual ~CNetCache_RequestHandlerHost() {}
    virtual void BeginReadTransmission() = 0;
    virtual void BeginDelayedWrite() = 0;
    virtual CNetCacheServer* GetServer() = 0;
    virtual SNetCache_RequestStat* GetStat() = 0;
    virtual const string* GetAuth() = 0;
    virtual void SetDiagParameters(const string& client_ip,
                                   const string& session_id) = 0;
};


class CNCReqParserException : public CException
{
public:
    enum EErrCode {
        /// Request has wrong format
        /// (e.g. parameter starts with quote but not ends with it)
        eWrongFormat,
        /// Empty line in protocol or invalid command
        eNotCommand,
        /// Wrong number of parameters to the command
        eWrongParamsCnt,
        /// Wrong format of command parameters
        eWrongParams
    };

    virtual const char* GetErrCodeString(void) const;

    NCBI_EXCEPTION_DEFAULT(CNCReqParserException, CException);
};


class CNCRequestParser
{
public:
	CNCRequestParser(const string& request);

    const string& GetCommand(void) const;

    size_t GetParamsCount(void) const;

    const string& GetParam(size_t param_num) const;

    int GetIntParam(size_t param_num) const;
    unsigned int GetUIntParam(size_t param_num) const;

    void ShiftParams(void);

private:
    vector<string> m_Params;
};


// NetCache request handler
class CNetCache_RequestHandler
{
public:
    enum ETransmission {
        eTransmissionMoreBuffers = 0,
        eTransmissionLastBuffer
    };
    CNetCache_RequestHandler(CNetCache_RequestHandlerHost* host);
    virtual ~CNetCache_RequestHandler() {}
    /// Process request
    virtual
    void ProcessRequest(CNCRequestParser&      parser,
                        SNetCache_RequestStat& stat) = 0;
    // Optional for transmission reader, to start reading
    // transmission call m_Host->BeginReadTransmission()
    virtual bool ProcessTransmission(const char* buf, size_t buf_size,
        ETransmission eot) { return false; }
    // Optional for delayed write handler, to start writing
    // call m_Host->BeginDelayedWrite()
    virtual bool ProcessWrite() { return false; }

    // Service for subclasses
    CSocket& GetSocket(void) { return *m_Socket; }
    void SetSocket(CSocket *socket) { m_Socket = socket; }
protected:
    // Service for specific imlemetations
    static
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg)
    {
        CNetCacheServer::WriteMsg(sock, prefix, msg);
    }

    void CheckParamsCount(const CNCRequestParser& parser, size_t need_cnt);

protected:
    CNetCache_RequestHandlerHost* m_Host;
    CNetCacheServer*              m_Server;
    SNetCache_RequestStat*        m_Stat;
    // Transitional - do not hold ownership
    const string*    m_Auth;
private:
    CSocket* m_Socket;
};



//////////////////////////////////////////////////////////////////////////
//     Inlines
//////////////////////////////////////////////////////////////////////////

inline const string&
CNCRequestParser::GetCommand(void) const
{
    return m_Params[0];
}

inline size_t
CNCRequestParser::GetParamsCount(void) const
{
    return m_Params.size() - 1;
}

inline const string&
CNCRequestParser::GetParam(size_t param_num) const
{
    ++param_num;
    if (param_num < m_Params.size())
        return m_Params[param_num];
    else
        return kEmptyStr;
}

inline int
CNCRequestParser::GetIntParam(size_t param_num) const
{
    return atoi(GetParam(param_num).c_str());
}

inline unsigned int
CNCRequestParser::GetUIntParam(size_t param_num) const
{
    return static_cast<unsigned int>(GetIntParam(param_num));
}

inline void
CNCRequestParser::ShiftParams(void)
{
    m_Params.erase(m_Params.begin());
}


END_NCBI_SCOPE

#endif /* NETCACHE_REQUEST_HANDLER__HPP */
