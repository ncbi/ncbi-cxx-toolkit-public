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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   The CServer part of netstoraged - declarations.
 *
 */

#ifndef NCBI_NETSTORAGE_SERVER_HPP
#define NCBI_NETSTORAGE_SERVER_HPP

#include "nst_server_parameters.hpp"

#include <connect/services/json_over_uttp.hpp>

#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

class CNetStorageServer : public CServer
{
public:
    static CNetStorageServer* GetInstance();

    virtual bool ShutdownRequested();

    void SetShutdownFlag(int signum);

    void SetNSTParameters(SNSTServerParameters& params, bool bool_param);
};

class CNetStorageConnectionHandler : public IServer_ConnectionHandler
{
public:
    CNetStorageConnectionHandler(CNetStorageServer* server);
    virtual ~CNetStorageConnectionHandler();

private:
    enum EReadMode {
        eReadMessages,
        eReadRawData
    };

    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;

    virtual void OnOpen();
    virtual void OnRead();
    virtual void OnWrite();

    bool ReadRawData(CUTTPReader::EStreamParsingEvent uttp_event);

    virtual void OnMessage(const CJsonNode& message,
        size_t* raw_data_size);
    void OnData(const void* data, size_t data_size);

    void SendMessage(const CJsonNode& message);
    void SendOutputBuffer();

    CNetStorageServer* m_Server;
    char* m_ReadBuffer;
    EReadMode m_ReadMode;
    CUTTPReader m_UTTPReader;
    CJsonOverUTTPReader m_JSONReader;

    char* m_WriteBuffer;
    CUTTPWriter m_UTTPWriter;
    CJsonOverUTTPWriter m_JSONWriter;

    CFastMutex m_OutputQueueMutex;
    vector<CJsonNode> m_OutputQueue;
};

class CNetStorageConnectionFactory : public IServer_ConnectionFactory
{
public:
    CNetStorageConnectionFactory(CNetStorageServer* server);

private:
    virtual IServer_ConnectionHandler* Create();

    CNetStorageServer* m_Server;
};

END_NCBI_SCOPE

#endif // NCBI_NETSTORAGE_SERVER_HPP
