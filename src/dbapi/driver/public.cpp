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


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
//  CCDB_Connection::
//

CDB_Connection::CDB_Connection(I_Connection* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid connection provided", 200001 );

    m_Connect = c;
    m_Connect->SetResultProcessor(0); // to clean up the result processor if any
    m_Connect->Acquire((CDB_BaseEnt**) &m_Connect);
}


bool CDB_Connection::IsAlive()
{
    return (m_Connect == 0) ? false : m_Connect->IsAlive();
}

#define CHECK_CONNECTION( conn ) \
    CHECK_DRIVER_WARNING( !conn, "Connection has been closed", 200002 )
    
// inline void s_CheckConnection(I_Connection* conn, const char* method_name)
// {
//     if ( !conn ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200002,
// //                            "CDB_Connection::" + string(method_name),
// //                            "Connection has been closed");
//         DATABASE_DRIVER_WARNING( "Connection has been closed", 200002 );
//     }
// }

CDB_LangCmd* CDB_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{

    CHECK_CONNECTION(m_Connect);
    return m_Connect->LangCmd(lang_query, nof_params);
}

CDB_RPCCmd* CDB_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->RPC(rpc_name, nof_args);
}

CDB_BCPInCmd* CDB_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->BCPIn(table_name, nof_columns);
}

CDB_CursorCmd* CDB_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->Cursor(cursor_name, query, nof_params, batch_size);
}

CDB_SendDataCmd* CDB_Connection::SendDataCmd(I_ITDescriptor& desc,
                                             size_t data_size, bool log_it)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->SendDataCmd(desc, data_size, log_it);
}

bool CDB_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt, bool log_it)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->SendData(desc, txt, log_it);
}

bool CDB_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->SendData(desc, img, log_it);
}

bool CDB_Connection::Refresh()
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->Refresh();
}

const string& CDB_Connection::ServerName() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->ServerName();
}

const string& CDB_Connection::UserName() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->UserName();
}

const string& CDB_Connection::Password() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->Password();
}

I_DriverContext::TConnectionMode  CDB_Connection::ConnectMode() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->ConnectMode();
}

bool CDB_Connection::IsReusable() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->IsReusable();
}

const string& CDB_Connection::PoolName() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->PoolName();
}

I_DriverContext* CDB_Connection::Context() const
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->Context();
}

void CDB_Connection::PushMsgHandler(CDB_UserHandler* h)
{
    CHECK_CONNECTION(m_Connect);
    m_Connect->PushMsgHandler(h);
}

void CDB_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    CHECK_CONNECTION(m_Connect);
    m_Connect->PopMsgHandler(h);
}

CDB_ResultProcessor*
CDB_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    return m_Connect? m_Connect->SetResultProcessor(rp) : 0;
}

CDB_Connection::~CDB_Connection()
{
    if ( m_Connect ) {
        m_Connect->Release();
        Context()->x_Recycle(m_Connect, m_Connect->IsReusable());
    }
}


bool CDB_Connection::Abort()
{
    CHECK_CONNECTION(m_Connect);
    return m_Connect->Abort();
}


////////////////////////////////////////////////////////////////////////////
//  CDB_Result::
//

CDB_Result::CDB_Result(I_Result* r)
{
    CHECK_DRIVER_ERROR( !r, "No valid result provided", 200004 );

    m_Res = r;
    m_Res->Acquire((CDB_BaseEnt**) &m_Res);
}


#define CHECK_RESULT( res ) \
    CHECK_DRIVER_WARNING( !res, "This result is not available anymore", 200003 )
    
// inline void s_CheckResult(I_Result* res, const char* method_name)
// {
//     if ( !res ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200003,
// //                            "CDB_Result::" + string(method_name),
// //                            "This result is not available anymore");
//         DATABASE_DRIVER_WARNING( "This result is not available anymore", 200003 );
//     }
// }


EDB_ResType CDB_Result::ResultType() const
{
    CHECK_RESULT(m_Res);
    return m_Res->ResultType();
}

unsigned int CDB_Result::NofItems() const
{
    CHECK_RESULT(m_Res);
    return m_Res->NofItems();
}

const char* CDB_Result::ItemName(unsigned int item_num) const
{
    CHECK_RESULT(m_Res);
    return m_Res->ItemName(item_num);
}

size_t CDB_Result::ItemMaxSize(unsigned int item_num) const
{
    CHECK_RESULT(m_Res);
    return m_Res->ItemMaxSize(item_num);
}

EDB_Type CDB_Result::ItemDataType(unsigned int item_num) const
{
    CHECK_RESULT(m_Res);
    return m_Res->ItemDataType(item_num);
}

bool CDB_Result::Fetch()
{
    CHECK_RESULT(m_Res);
    return m_Res->Fetch();
}

int CDB_Result::CurrentItemNo() const
{
    CHECK_RESULT(m_Res);
    return m_Res->CurrentItemNo();
}

CDB_Object* CDB_Result::GetItem(CDB_Object* item_buf)
{
    CHECK_RESULT(m_Res);
    return m_Res->GetItem(item_buf);
}

size_t CDB_Result::ReadItem(void* buffer, size_t buffer_size, bool* is_null)
{
    CHECK_RESULT(m_Res);
    return m_Res->ReadItem(buffer, buffer_size, is_null);
}

I_ITDescriptor* CDB_Result::GetImageOrTextDescriptor()
{
    CHECK_RESULT(m_Res);
    return m_Res->GetImageOrTextDescriptor();
}

bool CDB_Result::SkipItem()
{
    CHECK_RESULT(m_Res);
    return m_Res->SkipItem();
}


CDB_Result::~CDB_Result()
{
    if ( m_Res ) {
        m_Res->Release();
    }
}



////////////////////////////////////////////////////////////////////////////
//  CDB_LangCmd::
//

CDB_LangCmd::CDB_LangCmd(I_LangCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200004 );

    m_Cmd = c;
    m_Cmd->Acquire((CDB_BaseEnt**) &m_Cmd);
}


#define CHECK_COMMAND( cmd ) \
    CHECK_DRIVER_WARNING( !cmd, "This command cannot be used anymore", 200005 )
    
// inline void s_CheckLangCmd(I_LangCmd* cmd, const char* method_name)
// {
//     if ( !cmd ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200005,
// //                            "CDB_LangCmd::" + string(method_name),
// //                            "This command can not be used anymore");
//         DATABASE_DRIVER_WARNING( "This command cannot be used anymore", 200005 );
//     }
// }


bool CDB_LangCmd::More(const string& query_text)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->More(query_text);
}

bool CDB_LangCmd::BindParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->BindParam(param_name, pVal);
}

bool CDB_LangCmd::SetParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->SetParam(param_name, pVal);
}

bool CDB_LangCmd::Send()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Send();
}

bool CDB_LangCmd::WasSent() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->WasSent();
}

bool CDB_LangCmd::Cancel()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Cancel();
}

bool CDB_LangCmd::WasCanceled() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->WasCanceled();
}

CDB_Result* CDB_LangCmd::Result()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Result();
}

bool CDB_LangCmd::HasMoreResults() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->HasMoreResults();
}

bool CDB_LangCmd::HasFailed() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->HasFailed();
}

int CDB_LangCmd::RowCount() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->RowCount();
}

void CDB_LangCmd::DumpResults()
{
    CHECK_COMMAND( m_Cmd );
    m_Cmd->DumpResults();
}

CDB_LangCmd::~CDB_LangCmd()
{
    if ( m_Cmd ) {
        m_Cmd->Release();
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_RPCCmd::
//

CDB_RPCCmd::CDB_RPCCmd(I_RPCCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );
    m_Cmd = c;
    m_Cmd->Acquire((CDB_BaseEnt**) &m_Cmd);
}


// inline void s_CheckRPCCmd(I_RPCCmd* cmd, const char* method_name)
// {
//     if ( !cmd ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200005,
// //                            "CDB_RPCCmd::" + string(method_name),
// //                            "This command can not be used anymore");
//         DATABASE_DRIVER_WARNING( "This command cannot be used anymore", 200005 );
//     }
// }


bool CDB_RPCCmd::BindParam(const string& param_name, CDB_Object* pVal,
                           bool out_param)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->BindParam(param_name, pVal, out_param);
}

bool CDB_RPCCmd::SetParam(const string& param_name, CDB_Object* pVal,
                          bool out_param)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->SetParam(param_name, pVal, out_param);
}

bool CDB_RPCCmd::Send()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Send();
}

bool CDB_RPCCmd::WasSent() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->WasSent();
}

bool CDB_RPCCmd::Cancel()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Cancel();
}

bool CDB_RPCCmd::WasCanceled() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->WasCanceled();
}

CDB_Result* CDB_RPCCmd::Result()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Result();
}

bool CDB_RPCCmd::HasMoreResults() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->HasMoreResults();
}

bool CDB_RPCCmd::HasFailed() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->HasFailed();
}

int CDB_RPCCmd::RowCount() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->RowCount();
}

void CDB_RPCCmd::DumpResults()
{
    CHECK_COMMAND( m_Cmd );
    m_Cmd->DumpResults();
}

void CDB_RPCCmd::SetRecompile(bool recompile)
{
    CHECK_COMMAND( m_Cmd );
    m_Cmd->SetRecompile(recompile);
}


CDB_RPCCmd::~CDB_RPCCmd()
{
    if ( m_Cmd ) {
        m_Cmd->Release();
    }
}



////////////////////////////////////////////////////////////////////////////
//  CDB_BCPInCmd::
//

CDB_BCPInCmd::CDB_BCPInCmd(I_BCPInCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200007 );

    m_Cmd = c;
    m_Cmd->Acquire((CDB_BaseEnt**) &m_Cmd);
}


// inline void s_CheckBCPInCmd(I_BCPInCmd* cmd, const char* method_name)
// {
//     if ( !cmd ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200005,
// //                            "CDB_BCPInCmd::" + string(method_name),
// //                            "This command can not be used anymore");
//         DATABASE_DRIVER_WARNING( "This command cannot be used anymore", 200005 );
//     }
// }


bool CDB_BCPInCmd::Bind(unsigned int column_num, CDB_Object* pVal)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Bind(column_num, pVal);
}

bool CDB_BCPInCmd::SendRow()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->SendRow();
}

bool CDB_BCPInCmd::Cancel()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Cancel();
}

bool CDB_BCPInCmd::CompleteBatch()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->CompleteBatch();
}

bool CDB_BCPInCmd::CompleteBCP()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->CompleteBCP();
}


CDB_BCPInCmd::~CDB_BCPInCmd()
{
    if ( m_Cmd ) {
        m_Cmd->Release();
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_CursorCmd::
//

CDB_CursorCmd::CDB_CursorCmd(I_CursorCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );
    m_Cmd = c;
    m_Cmd->Acquire((CDB_BaseEnt**) &m_Cmd);
}


// inline void s_CheckCursorCmd(I_CursorCmd* cmd, const char* method_name)
// {
//     if ( !cmd ) {
// //         throw CDB_ClientEx(eDiag_Warning, 200005,
// //                            "CDB_CursorCmd::" + string(method_name),
// //                            "This command can not be used anymore");
//         DATABASE_DRIVER_WARNING( "This command cannot be used anymore", 200005 );
//     }
// }


bool CDB_CursorCmd::BindParam(const string& param_name, CDB_Object* pVal)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->BindParam(param_name, pVal);
}


CDB_Result* CDB_CursorCmd::Open()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Open();
}

bool CDB_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Update(table_name, upd_query);
}

bool CDB_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, bool log_it)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->UpdateTextImage(item_num, data, log_it);
}

CDB_SendDataCmd* CDB_CursorCmd::SendDataCmd(unsigned int item_num,
                                            size_t size, bool log_it)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->SendDataCmd(item_num, size, log_it);
}


bool CDB_CursorCmd::Delete(const string& table_name)
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Delete(table_name);
}

int CDB_CursorCmd::RowCount() const
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->RowCount();
}

bool CDB_CursorCmd::Close()
{
    CHECK_COMMAND( m_Cmd );
    return m_Cmd->Close();
}


CDB_CursorCmd::~CDB_CursorCmd()
{
    if ( m_Cmd ) {
        m_Cmd->Release();
    }
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_SendDataCmd::
//

CDB_SendDataCmd::CDB_SendDataCmd(I_SendDataCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );

    m_Cmd = c;
    m_Cmd->Acquire((CDB_BaseEnt**) &m_Cmd);
}


size_t CDB_SendDataCmd::SendChunk(const void* pChunk, size_t nofBytes)
{
    CHECK_DRIVER_WARNING( !m_Cmd, "This command cannot be used anymore", 200005 );

    return m_Cmd->SendChunk(pChunk, nofBytes);
}


CDB_SendDataCmd::~CDB_SendDataCmd()
{
    if (m_Cmd) {
        m_Cmd->Release();
    }
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

CDB_ResultProcessor::CDB_ResultProcessor(CDB_Connection* c)
{
    m_Con = c;
    if ( m_Con ) {
        m_Prev = m_Con->SetResultProcessor(this);
        if(m_Prev) m_Prev->Acquire((CDB_BaseEnt**)(&m_Prev));
        m_Con->Acquire((CDB_BaseEnt**)(&m_Con));
    }
}

void CDB_ResultProcessor::ProcessResult(CDB_Result& res)
{
    while (res.Fetch());  // fetch and forget
}


CDB_ResultProcessor::~CDB_ResultProcessor()
{
    if ( m_Con ) {
        m_Con->SetResultProcessor(m_Prev);
        if(m_Prev) m_Con->Acquire((CDB_BaseEnt**)(&m_Prev->m_Con));
        else m_Con->Release();
    }
    else if(m_Prev) m_Prev->m_Con= 0;

    if(m_Prev) m_Prev->Release();

}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
