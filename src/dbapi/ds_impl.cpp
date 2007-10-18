/* $Id$
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
* Author:  Michael Kholodov
*
* File Description:
*   DataSource implementation
*
*/

#include <ncbi_pch.hpp>
#include <dbapi/error_codes.hpp>
#include "ds_impl.hpp"
#include "conn_impl.hpp"
#include "err_handler.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls


BEGIN_NCBI_SCOPE


CDataSource::CDataSource(I_DriverContext *ctx)
    : m_loginTimeout(30), m_context(ctx), m_poolUsed(false),
      m_multiExH(0)
{
    SetIdent("CDataSource");
}

CDataSource::~CDataSource()
{
    try {
        _TRACE("Deleting " << GetIdent() << " " << (void*)this);
        Notify(CDbapiDeletedEvent(this));

        if (m_multiExH) {
            // Unregister a msg handler with a context ...
            m_context->PopCntxMsgHandler(m_multiExH);
            m_context->PopDefConnMsgHandler(m_multiExH);
        }

        delete m_multiExH;

        // We won't delete context unless all connections are closed.
        // This will cause a memory leak but it also will prevent from
        // accessing an already freed memory.
        if (m_context->NofConnections() == 0) {
            delete m_context;
        }

        _TRACE(GetIdent() << " " << (void*)this << " deleted.");
    }
    NCBI_CATCH_ALL_X( 5, kEmptyStr )
}

void CDataSource::SetLoginTimeout(unsigned int i)
{
    m_loginTimeout = i;
    if( m_context != 0 ) {
        m_context->SetLoginTimeout(i);
    }
}

void CDataSource::SetLogStream(CNcbiOstream* out)
{
    if( out != 0 ) {
        // Clear the previous handlers if present
        if( m_multiExH != 0 ) {
            m_context->PopCntxMsgHandler(m_multiExH);
            m_context->PopDefConnMsgHandler(m_multiExH);
            delete m_multiExH;
            _TRACE("SetLogStream(): CDataSource " << (void*)this
                << ": message handler " << (void*)m_multiExH
                << " removed from context " << (void*)m_context);
            m_multiExH = 0;
        }

        CDB_UserHandler *newH = new CDB_UserHandler_Stream(out);
        CDB_UserHandler *h = CDB_UserHandler::SetDefault(newH);
        delete h;
        _TRACE("SetLogStream(): CDataSource " << (void*)this
                << ": new default message handler " << (void*)newH
                << " installed");
   }
    else {
        if( m_multiExH == 0 ) {
            m_multiExH = new CToMultiExHandler;

            m_context->PushCntxMsgHandler(m_multiExH);
            m_context->PushDefConnMsgHandler(m_multiExH);
            _TRACE("SetLogStream(): CDataSource " << (void*)this
                << ": message handler " << (void*)m_multiExH
                << " installed on context " << (void*)m_context);
        }
    }
}

CToMultiExHandler* CDataSource::GetHandler()
{
    return m_multiExH;
}

CDB_MultiEx* CDataSource::GetErrorAsEx()
{
    return GetHandler() == 0 ? 0 : GetHandler()->GetMultiEx();
}

string CDataSource::GetErrorInfo()
{
    if( m_multiExH != 0 ) {
        CNcbiOstrstream out;
        CDB_UserHandler_Stream h(&out);
        h.HandleIt(GetHandler()->GetMultiEx());

        // Replace MultiEx
        GetHandler()->ReplaceMultiEx();
/*
        m_context->PopCntxMsgHandler(m_multiExH);
        m_context->PopDefConnMsgHandler(m_multiExH);
        delete m_multiExH;
        m_multiExH = new CToMultiExHandler;
        m_context->PushCntxMsgHandler(m_multiExH);
        m_context->PushDefConnMsgHandler(m_multiExH);
*/
        return CNcbiOstrstreamToString(out);
    }
    else
        return kEmptyStr;
}



I_DriverContext* CDataSource::GetDriverContext() {
    CHECK_NCBI_DBAPI(
        m_context == 0,
        "CDataSource::GetDriverContext(): no valid context"
        );

    return m_context;
}

const I_DriverContext* CDataSource::GetDriverContext() const
{
    //CHECK_NCBI_DBAPI(
    //    m_context == 0,
    //    "CDataSource::GetDriverContext(): no valid context"
    //    );

    //return m_context;
	return const_cast<CDataSource*>(this)->GetDriverContext();
}

IConnection* CDataSource::CreateConnection(EOwnership ownership)
{
    CConnection *conn = new CConnection(this, ownership);
    AddListener(conn);
    conn->AddListener(this);
    return conn;
}

void CDataSource::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName()
           << "' from " << e.GetSource()->GetIdent());

    if( dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
    }
}


END_NCBI_SCOPE
