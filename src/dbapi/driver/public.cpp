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
 * Author:  Vladimir Soussov
 *
 * File Description:  Data Server public interfaces
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
//  CCDB_Connection::
//

CDB_Connection::CDB_Connection(impl::CConnection* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid connection provided", 200001 );

    m_ConnImpl = c;
    m_ConnImpl->AttachTo(this);
    m_ConnImpl->SetResultProcessor(0); // to clean up the result processor if any
}


bool CDB_Connection::IsAlive()
{
    return (m_ConnImpl == 0) ? false : m_ConnImpl->IsAlive();
}

#define CHECK_CONNECTION( conn ) \
    CHECK_DRIVER_WARNING( !conn, "Connection has been closed", 200002 )

CDB_LangCmd* CDB_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->LangCmd(lang_query, nof_params);
}

CDB_RPCCmd* CDB_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->RPC(rpc_name, nof_args);
}

CDB_BCPInCmd* CDB_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->BCPIn(table_name, nof_columns);
}

CDB_CursorCmd* CDB_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Cursor(cursor_name, query, nof_params, batch_size);
}

CDB_SendDataCmd* CDB_Connection::SendDataCmd(I_ITDescriptor& desc,
                                             size_t data_size, bool log_it)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->SendDataCmd(desc, data_size, log_it);
}

bool CDB_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt, bool log_it)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->SendData(desc, txt, log_it);
}

bool CDB_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->SendData(desc, img, log_it);
}

bool CDB_Connection::Refresh()
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Refresh();
}

const string& CDB_Connection::ServerName() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->ServerName();
}

const string& CDB_Connection::UserName() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->UserName();
}

const string& CDB_Connection::Password() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Password();
}

I_DriverContext::TConnectionMode  CDB_Connection::ConnectMode() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->ConnectMode();
}

bool CDB_Connection::IsReusable() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->IsReusable();
}

const string& CDB_Connection::PoolName() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->PoolName();
}

I_DriverContext* CDB_Connection::Context() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Context();
}

void CDB_Connection::PushMsgHandler(CDB_UserHandler* h,
                                    EOwnership ownership)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->PushMsgHandler(h, ownership);
}

void CDB_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->PopMsgHandler(h);
}

CDB_ResultProcessor*
CDB_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    return m_ConnImpl? m_ConnImpl->SetResultProcessor(rp) : NULL;
}

CDB_Connection::~CDB_Connection()
{
    try {
        if ( m_ConnImpl ) {
            m_ConnImpl->Release();
            m_ConnImpl = NULL;
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CDB_Connection::Abort()
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Abort();
}

bool CDB_Connection::Close(void)
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Close();
}

////////////////////////////////////////////////////////////////////////////
//  CDB_Result::
//

CDB_Result::CDB_Result(impl::CResult* r) :
    m_ResImpl(r)
{
    CHECK_DRIVER_ERROR( !m_ResImpl, "No valid result provided", 200004 );

    m_ResImpl->AttachTo(this);
}


#define CHECK_RESULT( res ) \
    CHECK_DRIVER_WARNING( !res, "This result is not available anymore", 200003 )


EDB_ResType CDB_Result::ResultType() const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().ResultType();
}

unsigned int CDB_Result::NofItems() const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().NofItems();
}

const char* CDB_Result::ItemName(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().ItemName(item_num);
}

size_t CDB_Result::ItemMaxSize(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().ItemMaxSize(item_num);
}

EDB_Type CDB_Result::ItemDataType(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().ItemDataType(item_num);
}

bool CDB_Result::Fetch()
{
    // An exception should be thrown from this place. We cannot omit this exception
    // because it is expected by ftds and ftds63 drivers in CursorResult::Fetch.
    CHECK_RESULT( GetIResultPtr() );
//     if ( !GetIResultPtr() ) {
//         return false;
//     }
    return GetIResult().Fetch();
}

int CDB_Result::CurrentItemNo() const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().CurrentItemNo();
}

int CDB_Result::GetColumnNum(void) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetColumnNum();
}

CDB_Object* CDB_Result::GetItem(CDB_Object* item_buf)
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetItem(item_buf);
}

size_t CDB_Result::ReadItem(void* buffer, size_t buffer_size, bool* is_null)
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().ReadItem(buffer, buffer_size, is_null);
}

I_ITDescriptor* CDB_Result::GetImageOrTextDescriptor()
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetImageOrTextDescriptor();
}

bool CDB_Result::SkipItem()
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().SkipItem();
}


CDB_Result::~CDB_Result()
{
    try {
        if ( GetIResultPtr() ) {
            GetIResult().Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



////////////////////////////////////////////////////////////////////////////
//  CDB_LangCmd::
//

CDB_LangCmd::CDB_LangCmd(impl::CBaseCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200004 );

    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


#define CHECK_COMMAND( cmd ) \
    CHECK_DRIVER_WARNING( !cmd, "This command cannot be used anymore", 200005 )


bool CDB_LangCmd::More(const string& query_text)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->More(query_text);
}

bool CDB_LangCmd::BindParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->BindParam(param_name, pVal);
}

bool CDB_LangCmd::SetParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->SetParam(param_name, pVal);
}

bool CDB_LangCmd::Send()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Send();
}

bool CDB_LangCmd::WasSent() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->WasSent();
}

bool CDB_LangCmd::Cancel()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Cancel();
}

bool CDB_LangCmd::WasCanceled() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->WasCanceled();
}

CDB_Result* CDB_LangCmd::Result()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Result();
}

bool CDB_LangCmd::HasMoreResults() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->HasMoreResults();
}

bool CDB_LangCmd::HasFailed() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->HasFailed();
}

int CDB_LangCmd::RowCount() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->RowCount();
}

void CDB_LangCmd::DumpResults()
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->DumpResults();
}

CDB_LangCmd::~CDB_LangCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_RPCCmd::
//

CDB_RPCCmd::CDB_RPCCmd(impl::CBaseCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );
    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


bool CDB_RPCCmd::BindParam(const string& param_name, CDB_Object* pVal,
                           bool out_param)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->BindParam(param_name, pVal, out_param);
}

bool CDB_RPCCmd::SetParam(const string& param_name, CDB_Object* pVal,
                          bool out_param)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->SetParam(param_name, pVal, out_param);
}

bool CDB_RPCCmd::Send()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Send();
}

bool CDB_RPCCmd::WasSent() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->WasSent();
}

bool CDB_RPCCmd::Cancel()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Cancel();
}

bool CDB_RPCCmd::WasCanceled() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->WasCanceled();
}

CDB_Result* CDB_RPCCmd::Result()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Result();
}

bool CDB_RPCCmd::HasMoreResults() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->HasMoreResults();
}

bool CDB_RPCCmd::HasFailed() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->HasFailed();
}

int CDB_RPCCmd::RowCount() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->RowCount();
}

void CDB_RPCCmd::DumpResults()
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->DumpResults();
}

void CDB_RPCCmd::SetRecompile(bool recompile)
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->SetRecompile(recompile);
}

const string& CDB_RPCCmd::GetProcName(void) const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetQuery();
}


CDB_RPCCmd::~CDB_RPCCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



////////////////////////////////////////////////////////////////////////////
//  CDB_BCPInCmd::
//

CDB_BCPInCmd::CDB_BCPInCmd(impl::CBaseCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200007 );

    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


bool CDB_BCPInCmd::Bind(unsigned int column_num, CDB_Object* pVal)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Bind(column_num, pVal);
}

bool CDB_BCPInCmd::SendRow()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Send();
}

bool CDB_BCPInCmd::Cancel()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Cancel();
}

bool CDB_BCPInCmd::CompleteBatch()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->CommitBCPTrans();
}

bool CDB_BCPInCmd::CompleteBCP()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->EndBCP();
}


CDB_BCPInCmd::~CDB_BCPInCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_CursorCmd::
//

CDB_CursorCmd::CDB_CursorCmd(impl::CCursorCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );
    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


bool CDB_CursorCmd::BindParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->BindParam(param_name, pVal);
}


CDB_Result* CDB_CursorCmd::Open()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Open();
}

bool CDB_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Update(table_name, upd_query);
}

bool CDB_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, bool log_it)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->UpdateTextImage(item_num, data, log_it);
}

CDB_SendDataCmd* CDB_CursorCmd::SendDataCmd(unsigned int item_num,
                                            size_t size, bool log_it)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->SendDataCmd(item_num, size, log_it);
}


bool CDB_CursorCmd::Delete(const string& table_name)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Delete(table_name);
}

int CDB_CursorCmd::RowCount() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->RowCount();
}

bool CDB_CursorCmd::Close()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Close();
}


CDB_CursorCmd::~CDB_CursorCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_SendDataCmd::
//

CDB_SendDataCmd::CDB_SendDataCmd(impl::CSendDataCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );

    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


size_t CDB_SendDataCmd::SendChunk(const void* pChunk, size_t nofBytes)
{
    CHECK_DRIVER_WARNING( !m_CmdImpl, "This command cannot be used anymore", 200005 );

    return m_CmdImpl->SendChunk(pChunk, nofBytes);
}


bool CDB_SendDataCmd::Cancel(void)
{
    CHECK_DRIVER_WARNING( !m_CmdImpl, "This command cannot be used anymore", 200005 );

    return m_CmdImpl->Cancel();
}

CDB_SendDataCmd::~CDB_SendDataCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_ITDescriptor::
//

int CDB_ITDescriptor::DescriptorType() const
{
    return 0;
}

CDB_ITDescriptor::~CDB_ITDescriptor()
{
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_ResultProcessor::
//

CDB_ResultProcessor::CDB_ResultProcessor(CDB_Connection* c) :
    m_Con(NULL),
    m_Prev(NULL),
    m_Next(NULL)
{
    SetConn(c);
}

void CDB_ResultProcessor::ProcessResult(CDB_Result& res)
{
    while (res.Fetch())  // fetch and forget
        continue;
}


CDB_ResultProcessor::~CDB_ResultProcessor()
{
    try {
        if ( m_Con ) {
            m_Con->SetResultProcessor(m_Prev);
        }

        if(m_Prev) {
            m_Prev->m_Next = m_Next;
        }

        if(m_Next) {
            m_Next->m_Prev = m_Prev;
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

void CDB_ResultProcessor::SetConn(CDB_Connection* c)
{
    // Clear previously used connection ...
    if ( m_Con ) {
        m_Con->SetResultProcessor(NULL);
    }

    m_Con = c;

    if ( m_Con ) {
        _ASSERT(m_Prev == NULL);
        m_Prev = m_Con->SetResultProcessor(this);

        if (m_Prev) {
            _ASSERT(m_Prev->m_Next == NULL);
            m_Prev->m_Next = this;
        }
    }
}

void CDB_ResultProcessor::ReleaseConn(void)
{
    m_Con = NULL;
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.30  2006/11/20 18:14:15  ssikorsk
 * Implemented CDB_RPCCmd::GetProcName.
 *
 * Revision 1.29  2006/07/18 15:47:58  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.28  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.27  2006/06/05 21:03:02  ssikorsk
 * Implemented CDB_Connection::Release.
 *
 * Revision 1.26  2006/06/05 18:05:28  ssikorsk
 * Implemented CDB_SendDataCmd::Cancel.
 *
 * Revision 1.25  2006/06/02 20:58:52  ssikorsk
 * Deleted CDB_Result::SetIResult;
 *
 * Revision 1.24  2006/06/02 19:29:42  ssikorsk
 * Renamed CDB_Result::m_Res to m_IRes;
 * Renamed CDB_Result::GetResultSet to GetIResultPtr;
 * Renamed CDB_Result::SetResultSet to SetIResult;
 * Added CDB_Result::GetIResult;
 *
 * Revision 1.23  2006/05/15 19:34:42  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.22  2006/04/05 14:26:53  ssikorsk
 * Implemented CDB_Connection::Close
 *
 * Revision 1.21  2005/12/06 19:27:31  ssikorsk
 * Revamp code to use GetResultSet/SetResultSet methods
 * instead of raw data access.
 *
 * Revision 1.20  2005/11/02 14:32:39  ssikorsk
 * Use
 macro instead of catch(...)
 *
 * Revision 1.19  2005/10/26 12:25:15  ssikorsk
 * CDB_Result::Fetch returns false if m_Res is NULL now.
 *
 * Revision 1.18  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.17  2005/09/07 11:05:37  ssikorsk
 * Added a CDB_Result::GetColumnNum implementation
 *
 * Revision 1.16  2005/08/09 13:15:45  ssikorsk
 * Removed redudant comments
 *
 * Revision 1.15  2005/07/14 19:24:17  ucko
 * Revert R1.13, as the corresponding header change has been reverted
 * (with the source again apparently forgotten)
 *
 * Revision 1.14  2005/07/07 19:25:39  ssikorsk
 * A few changes to support new ftds driver
 *
 * Revision 1.13  2005/07/07 16:52:16  ucko
 * Replaced ProcessResult(CDB_Result& res) with ProcessResult(I_Result& res)
 *
 * Revision 1.12  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.11  2005/02/23 21:37:43  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.10  2004/06/08 18:55:40  soussov
 * adds code which cleans up a result processor if any in reusable connection
 *
 * Revision 1.9  2004/06/08 18:17:42  soussov
 * adds code which protects result processor from hung pointers to connection and/or other processor
 *
 * Revision 1.8  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.7  2003/06/20 19:11:25  vakatov
 * CDB_ResultProcessor::
 *  - added MS-Win DLL export macro
 *  - made the destructor virtual
 *  - moved code from the header to the source file
 *  - formally reformatted the code
 *
 * Revision 1.6  2003/06/05 15:58:38  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method
 * for Connection interface, adds CDB_ResultProcessor class
 *
 * Revision 1.5  2003/01/29 22:35:05  soussov
 * replaces const string& with const char* in inlines
 *
 * Revision 1.4  2002/03/26 15:32:24  soussov
 * new image/text operations added
 *
 * Revision 1.3  2001/11/06 17:59:54  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.1  2001/09/21 23:40:00  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
