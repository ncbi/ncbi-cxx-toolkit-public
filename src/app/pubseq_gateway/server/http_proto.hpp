#ifndef HTTP_PROTO__HPP
#define HTTP_PROTO__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <string>
using namespace std;

#include <h2o.h>

#include "pubseq_gateway_logging.hpp"

class CHttpProto;
class CHttpDaemon;
class CHttpRequestParser;
class CHttpPostParser;
class CHttpConnection;
class CHttpRequest;
class CPSGS_Reply;
class CTcpDaemon;
struct CTcpWorker;

void CheckFDLimit(void);


#if H2O_VERSION_MAJOR == 2 && H2O_VERSION_MINOR >= 3
    h2o_socket_t *  Geth2oConnectionSocketForRequest(h2o_req_t *  req);
#endif


using HttpHandlerFunction_t = std::function<void(CHttpRequest &  req,
                                                 shared_ptr<CPSGS_Reply>  reply)>;

struct CHttpGateHandler
{
    void Init(HttpHandlerFunction_t *  handler,
              CTcpDaemon *  tcpd,
              CHttpDaemon *  httpd,
              CHttpRequestParser *  get_parser,
              CHttpPostParser *  post_parser)
    {
        m_Handler = handler;
        m_Tcpd = tcpd;
        m_Httpd = httpd;
        m_GetParser = get_parser;
        m_PostParser = post_parser;
    }

    struct st_h2o_handler_t     m_H2oHandler; // must be first
    HttpHandlerFunction_t *     m_Handler;
    CTcpDaemon *                m_Tcpd;
    CHttpDaemon *               m_Httpd;
    CHttpRequestParser *        m_GetParser;
    CHttpPostParser *           m_PostParser;
};


class CHttpProto
{
public:
    CHttpProto(CHttpDaemon & daemon) :
        m_Worker(nullptr),
        m_Daemon(daemon)
    {
        // PSG_TRACE("CHttpProto::CHttpProto");
    }

    ~CHttpProto()
    {
        // PSG_TRACE("~CHttpProto");
    }

    void BeforeStart(void)
    {}

    void ThreadStart(uv_loop_t *  loop, CTcpWorker *  worker);
    void ThreadStop(void);

    void OnNewConnection(uv_stream_t *  conn,
                         CHttpConnection *  http_conn,
                         uv_close_cb  close_cb);
    void OnClientClosedConnection(uv_stream_t *  conn,
                                  CHttpConnection *  http_conn);
    void WakeWorker(void);
    void OnTimer(void);
    void OnAsyncWork(bool  cancel);

    static void DaemonStarted() {}
    static void DaemonStopped() {}

    int OnHttpRequest(CHttpGateHandler *  rh,
                      h2o_req_t *  req,
                      const char *  cd_uid);

private:
    CTcpWorker *        m_Worker;
    CHttpDaemon &       m_Daemon;
};

#endif

