#ifndef NETSTORAGE_HANDLER__HPP
#define NETSTORAGE_HANDLER__HPP

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
 * Authors:  Denis Vakatov
 *
 * File Description: NetStorage commands handler
 *
 */

#include <string>
#include <connect/services/json_over_uttp.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <connect/server.hpp>


BEGIN_NCBI_SCOPE

// Forward declarations
class CNetStorageServer;




class CNetStorageHandler : public IServer_LineMessageHandler
{
public:
    CNetStorageHandler(CNetStorageServer *  server);
    ~CNetStorageHandler();

    // MessageHandler protocol
    virtual void      OnOpen(void);
    virtual void      OnWrite(void);
    virtual void      OnClose(IServer_ConnectionHandler::EClosePeer peer);
    virtual void      OnTimeout(void);
    virtual void      OnOverflow(EOverflowReason reason);
    virtual void      OnMessage(BUF buffer);

    // Statuses of commands to be set in diagnostics' request context
    // Additional statuses can be taken from
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    enum EHTTPStatus {
        eStatus_OK                  = 200, //< Command is ok and execution is good

        eStatus_BadRequest          = 400, //< Command is incorrect
        eStatus_NotFound            = 404, //< Job is not found
        eStatus_Inactive            = 408, //< Connection was closed due to inactivity
                                           //< timeout
        eStatus_InvalidJobStatus    = 409, //< Invalid job status
        eStatus_HTTPProbe           = 444, //< Routine test from systems
        eStatus_SocketIOError       = 499, //< Error writing to socket

        eStatus_ServerError         = 500, //< Internal server error
        eStatus_NotImplemented      = 501, //< Command is not implemented
        eStatus_SubmitRefused       = 503, //< In refuse submits mode and received SUBMIT
        eStatus_ShuttingDown        = 503  //< Server is shutting down
    };


private:
    void x_SetQuickAcknowledge(void);
    void x_SetCmdRequestStatus(unsigned int  status)
    { if (m_CmdContext.NotNull())
        m_CmdContext->SetRequestStatus(status); }
    void x_SetConnRequestStatus(unsigned int  status)
    { if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(status); }

private:
    void x_CreateConnContext(void);
    unsigned int  x_GetPeerAddress(void);

    CNetStorageServer *         m_Server;

    // Diagnostics context for the current connection
    CRef<CRequestContext>       m_ConnContext;
    // Diagnostics context for the currently executed command
    CRef<CRequestContext>       m_CmdContext;

}; // CNetStorageHandler


END_NCBI_SCOPE

#endif

