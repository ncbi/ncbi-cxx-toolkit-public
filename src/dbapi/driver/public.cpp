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
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_DataServer


BEGIN_NCBI_SCOPE



#ifdef _DEBUG
static void s_TraceParams(const CDBParams& params,
                          const CDiagCompileInfo& info) {
    if (dynamic_cast<const impl::CCachedRowInfo*>(&params)) {
        return;
    }
    for (unsigned int i = 0, n = params.GetNum();  i < n;  ++i) {
        const CDB_Object* obj   = params.GetValue(i);
        const string&     name  = params.GetName(i);
        string            value = obj ? obj->GetLogString() : string("(null)");
        if (name.empty()) { // insertion, typically bulk
            CNcbiDiag(info, eDiag_Trace).GetRef()
                << "Column #" << (i + 1) << " = " << value
                << Endm;
        } else {
            CNcbiDiag(info, eDiag_Trace).GetRef()
                << "Parameter #" << (i + 1) << " (" << name << ") = " << value
                << Endm;
        }
    }
}
// Cite caller.
#  define TRACE_PARAMS(params) s_TraceParams(params, DIAG_COMPILE_INFO)
#else
#  define TRACE_PARAMS(params) ((void)0)
#endif



////////////////////////////////////////////////////////////////////////////
//  CDBParamVariant::
//
inline
unsigned int ConvertI2UI(int value)
{
    CHECK_DRIVER_ERROR( (value < 0), "Negative parameter's position not allowed.", 200001 );

    return static_cast<unsigned int>(value);
}

CDBParamVariant::CDBParamVariant(int pos)
: m_IsPositional(true)
, m_Pos(ConvertI2UI(pos))
{
}


CDBParamVariant::CDBParamVariant(unsigned int pos)
: m_IsPositional(true)
, m_Pos(pos)
{
}


CDBParamVariant::CDBParamVariant(const char* name)
: m_IsPositional(false)
, m_Pos(0)
, m_Name(MakeName(name, m_Format))
{
}

CDBParamVariant::CDBParamVariant(const string& name)
: m_IsPositional(false)
, m_Pos(0)
, m_Name(MakeName(name, m_Format))
{
}


CDBParamVariant::~CDBParamVariant(void)
{
}

// Not finished yet ...
string 
CDBParamVariant::GetName(CDBParamVariant::ENameFormat format) const
{
    if (format != GetFormat()) {
        switch (format) {
        case ePlainName:
            return MakePlainName(m_Name);
        case eQMarkName:    // '...WHERE name=?'
            return "?";
        case eNumericName:  // '...WHERE name=:1'
        case eNamedName:    // '...WHERE name=:name'
            return ':' + MakePlainName(m_Name);
        case eFormatName:   // ANSI C printf format codes, e.g. '...WHERE name=%s'
            return '%' + MakePlainName(m_Name);
        case eSQLServerName: // '...WHERE name=@name'
            return '@' + MakePlainName(m_Name);
        }
    }

    return m_Name;
}


CTempString CDBParamVariant::MakeName(const CTempString& name,
                                      CDBParamVariant::ENameFormat& format)
{
    // Do not make copy of name to process it ...

    CTempString new_name;
    CTempString::const_iterator begin_str = NULL, c = name.data();

    format = ePlainName;

    for (;  c != NULL  &&  c != name.end();  ++c) {
        char ch = *c;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (begin_str == NULL) {
                // Remove whitespace ...
                continue;
            } else {
                // Look forward for non-space characters.
                bool space_chars_only = true;
                for (const char* tc = c; tc != NULL && *tc != '\0'; ++tc) {
                    char tch = *tc;
                    if (tch == ' ' || tch == '\t' || tch == '\n' || tch == '\r') {
                        continue;
                    } else {
                        space_chars_only = false;
                        break;
                    }
                }

                if (space_chars_only) {
                    // Remove trailing whitespace ...
                    break;
                }
            }
        }
        // Check for leading symbol ...
        if (begin_str == NULL) {
            begin_str = c;

            switch (ch) {
                case '?' :
                    format = eQMarkName;
                    break;
                case ':' :
                    if (*(c + 1)) {
                        if (isdigit(*(c + 1))) {
                            format = eNumericName;
                        } else {
                            format = eNamedName;
                        }
                    } else {
                        DATABASE_DRIVER_ERROR("Invalid parameter format: "
                                              + string(name), 1);
                    }
                    break;
                case '@' :
                    format = eSQLServerName;
                    break;
                case '%' :
                    format = eFormatName;
                    break;
                case '$' :
                    // !!!!
                    format = eFormatName;
                    break;
            }
        }
    }

    if (begin_str != NULL) {
        new_name.assign(begin_str, c - begin_str);
    }

    return new_name;
}


string CDBParamVariant::MakePlainName(const CTempString& name)
{
    // Do not make copy of name to process it ...

    CTempString plain_name;
    CTempString::const_iterator begin_str = NULL, c = name.data();

    for (;  c != NULL  &&  c != name.end();  ++c) {
        char ch = *c;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (begin_str == NULL) {
                // Remove whitespaces ...
                continue;
            } else {
                // Look forward for non-space characters.
                bool space_chars_only = true;
                for (const char* tc = c; tc != NULL && *tc != '\0'; ++tc) {
                    char tch = *tc;
                    if (tch == ' ' || tch == '\t' || tch == '\n' || tch == '\r') {
                        continue;
                    } else {
                        space_chars_only = false;
                        break;
                    }
                }

                if (space_chars_only) {
                    // Remove trailing whitespace ...
                    break;
                }
            }
        }
        // Check for leading symbol ...
        if (begin_str == NULL) {
            begin_str = c;
            if (ch == ':' || ch == '@' || ch == '$' || ch == '%') {
                // Skip leading symbol ...
                ++begin_str;
            }
        }
    }

    if (begin_str != NULL) {
        plain_name.assign(begin_str, c - begin_str);
    }

    return plain_name;
}


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

CDB_LangCmd* CDB_Connection::LangCmd(const string& lang_query)
{
    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Sending SQL: " << lang_query);
    return m_ConnImpl->LangCmd(lang_query);
}

CDB_RPCCmd* CDB_Connection::RPC(const string& rpc_name)
{
    CHECK_CONNECTION(m_ConnImpl);
    // _TRACE-d in CDB_RPCCmd::Send, which performs the actual I/O.
    return m_ConnImpl->RPC(rpc_name);
}

CDB_BCPInCmd* CDB_Connection::BCPIn(const string& table_name)
{
    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Performing bulk insertion into " << table_name);
    return m_ConnImpl->BCPIn(table_name);
}

CDB_CursorCmd* CDB_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  batch_size)
{
    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Opening cursor for " << query);
    return m_ConnImpl->Cursor(cursor_name, query, batch_size);
}

CDB_SendDataCmd* CDB_Connection::SendDataCmd(I_ITDescriptor& desc,
                                             size_t data_size,
                                             bool log_it,
                                             bool dump_results)
{
    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Sending " << data_size << " byte(s) of data");
    return m_ConnImpl->SendDataCmd(desc, data_size, log_it, dump_results);
}

bool CDB_Connection::SendData(I_ITDescriptor& desc, CDB_Stream& lob, bool log_it)
{
    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Sending " << lob.Size() << " byte(s) of data");
    return m_ConnImpl->SendData(desc, lob, log_it);
}

void CDB_Connection::SetDatabaseName(const string& name)
{
    if (name.empty()) {
        return;
    }

    CHECK_CONNECTION(m_ConnImpl);
    _TRACE("Now using database " << name);
    m_ConnImpl->SetDatabaseName(name);
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

Uint4 CDB_Connection::Host() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Host();
}

Uint2 CDB_Connection::Port() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->Port();
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

const string& CDB_Connection::DatabaseName() const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->GetDatabaseName();
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
            Close();
        }
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


bool CDB_Connection::Abort()
{
    CHECK_CONNECTION(m_ConnImpl);
    if (m_ConnImpl->Abort()) {
        Close();
        return true;
    }
    return false;
}


bool CDB_Connection::Close(void)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->Release();
    m_ConnImpl = NULL;
    return true;
}

void CDB_Connection::SetTimeout(size_t nof_secs)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->SetTimeout(nof_secs);
}

void CDB_Connection::SetCancelTimeout(size_t nof_secs)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->SetCancelTimeout(nof_secs);
}

I_ConnectionExtra& CDB_Connection::GetExtraFeatures(void)
{
    CHECK_CONNECTION(m_ConnImpl);
    return *m_ConnImpl;
}

string CDB_Connection::GetDriverName(void) const
{
    CHECK_CONNECTION(m_ConnImpl);
    return m_ConnImpl->GetDriverName();
}

void CDB_Connection::FinishOpening(void)
{
    CHECK_CONNECTION(m_ConnImpl);
    m_ConnImpl->FinishOpening();
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


const CDBParams& CDB_Result::GetDefineParams(void) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetDefineParams();
}


unsigned int CDB_Result::NofItems() const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetDefineParams().GetNum();
}

const char* 
CDB_Result::ItemName(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    
    const string& name = GetIResult().GetDefineParams().GetName(item_num);
    
    if (!name.empty()) {
        return name.c_str();
    }

    return NULL;
}

size_t CDB_Result::ItemMaxSize(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetDefineParams().GetMaxSize(item_num);
}

EDB_Type CDB_Result::ItemDataType(unsigned int item_num) const
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetDefineParams().GetDataType(item_num);
}

bool CDB_Result::Fetch()
{
    // An exception should be thrown from this place. We cannot omit this exception
    // because it is expected by ftds driver in CursorResult::Fetch.
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

CDB_Object* CDB_Result::GetItem(CDB_Object* item_buf, EGetItem policy)
{
    CHECK_RESULT( GetIResultPtr() );
    return GetIResult().GetItem(item_buf, policy);
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
    NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )
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

CDBParams& CDB_LangCmd::GetBindParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetBindParams();
}


CDBParams& CDB_LangCmd::GetDefineParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetDefineParams();
}

bool CDB_LangCmd::Send()
{
    CHECK_COMMAND( m_CmdImpl );
    TRACE_PARAMS(m_CmdImpl->GetBindParams());
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
    NCBI_CATCH_ALL_X( 4, NCBI_CURRENT_FUNCTION )
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


CDBParams& CDB_RPCCmd::GetBindParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetBindParams();
}


CDBParams& CDB_RPCCmd::GetDefineParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetDefineParams();
}


bool CDB_RPCCmd::Send()
{
    CHECK_COMMAND( m_CmdImpl );
    _TRACE("Calling remote procedure " << GetProcName());
    TRACE_PARAMS(m_CmdImpl->GetBindParams());
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
    NCBI_CATCH_ALL_X( 5, NCBI_CURRENT_FUNCTION )
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


void CDB_BCPInCmd::SetHints(CTempString hints)
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->SetHints(hints);
}


void CDB_BCPInCmd::AddHint(EBCP_Hints hint, unsigned int value /* = 0 */)
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->AddHint(hint, value);
}


void CDB_BCPInCmd::AddOrderHint(CTempString columns)
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->AddOrderHint(columns);
}


CDBParams& CDB_BCPInCmd::GetBindParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetBindParams();
}


bool CDB_BCPInCmd::Bind(unsigned int column_num, CDB_Object* value)
{
    GetBindParams().Bind(column_num, value);
    return true;
}


bool CDB_BCPInCmd::SendRow()
{
    CHECK_COMMAND( m_CmdImpl );
    if (m_CmdImpl->m_RowsSent++ == 0) {
        TRACE_PARAMS(m_CmdImpl->GetBindParams());
    }
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
    if (m_CmdImpl->m_BatchesSent++ == 0  &&  m_CmdImpl->m_RowsSent > 1) {
        _TRACE("Sent a batch of " << m_CmdImpl->m_RowsSent << " rows");
    }
    return m_CmdImpl->CommitBCPTrans();
}

bool CDB_BCPInCmd::CompleteBCP()
{
    CHECK_COMMAND( m_CmdImpl );
    if (m_CmdImpl->m_BatchesSent > 1) {
        _TRACE("Sent " << m_CmdImpl->m_RowsSent << " rows, in "
               << m_CmdImpl->m_BatchesSent << " batches");
    }
    return m_CmdImpl->EndBCP();
}


CDB_BCPInCmd::~CDB_BCPInCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL_X( 6, NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_CursorCmd::
//

CDB_CursorCmd::CDB_CursorCmd(impl::CBaseCmd* c)
{
    CHECK_DRIVER_ERROR( !c, "No valid command provided", 200006 );
    m_CmdImpl = c;
    m_CmdImpl->AttachTo(this);
}


CDBParams& CDB_CursorCmd::GetBindParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetBindParams();
}


CDBParams& CDB_CursorCmd::GetDefineParams(void)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->GetDefineParams();
}


CDB_Result* CDB_CursorCmd::Open()
{
    CHECK_COMMAND( m_CmdImpl );
    TRACE_PARAMS(m_CmdImpl->GetBindParams());
    return m_CmdImpl->OpenCursor();
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
                                            size_t size,
                                            bool log_it,
                                            bool dump_results)
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->SendDataCmd(item_num, size, log_it, dump_results);
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
    return m_CmdImpl->CloseCursor();
}


CDB_CursorCmd::~CDB_CursorCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL_X( 7, NCBI_CURRENT_FUNCTION )
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

CDB_Result* CDB_SendDataCmd::Result()
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->Result();
}

bool CDB_SendDataCmd::HasMoreResults() const
{
    CHECK_COMMAND( m_CmdImpl );
    return m_CmdImpl->HasMoreResults();
}

void CDB_SendDataCmd::DumpResults()
{
    CHECK_COMMAND( m_CmdImpl );
    m_CmdImpl->DumpResults();
}

CDB_SendDataCmd::~CDB_SendDataCmd()
{
    try {
        if ( m_CmdImpl ) {
            m_CmdImpl->Release();
        }
    }
    NCBI_CATCH_ALL_X( 8, NCBI_CURRENT_FUNCTION )
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_ITDescriptor::
//

CDB_ITDescriptor::CDB_ITDescriptor(const string& table_name,
                                   const string& column_name,
                                   const string& search_conditions,
                                   ETDescriptorType column_type)
: m_TableName(table_name)
, m_ColumnName(column_name)
, m_SearchConditions(search_conditions)
, m_ColumnType(column_type)
{
}

CDB_ITDescriptor::~CDB_ITDescriptor()
{
}

int CDB_ITDescriptor::DescriptorType() const
{
    return 0;
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
    NCBI_CATCH_ALL_X( 9, NCBI_CURRENT_FUNCTION )
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


////////////////////////////////////////////////////////////////////////////////
CAutoTrans::CAutoTrans(CDB_Connection& connection)
: m_Abort(true)
, m_Conn(connection)
, m_TranCount(0)
{
    BeginTransaction();
    m_TranCount = GetTranCount();
}


CAutoTrans::~CAutoTrans(void)
{
    try
    {
        const int curr_TranCount = GetTranCount();

        if (curr_TranCount >= m_TranCount) {
            if (curr_TranCount > m_TranCount) {
                // A nested transaction is started and not finished yet ...
                ERR_POST_X(1, Warning << "A nested transaction was started and "
                                         "it is not finished yet.");
            }

            // Assume that we are on the same level of transaction nesting.
            if(m_Abort) {
                Rollback();
            } else {
                Commit();
            }
        }

        // Skip commit/rollback if this transaction was previously
        // explicitly finished ...
    }
    NCBI_CATCH_ALL_X( 10, NCBI_CURRENT_FUNCTION )
}

void
CAutoTrans::BeginTransaction(void)
{
    auto_ptr<CDB_LangCmd> auto_stmt(m_Conn.LangCmd("BEGIN TRANSACTION"));
    auto_stmt->Send();
    auto_stmt->DumpResults();
}


void
CAutoTrans::Commit(void)
{
    auto_ptr<CDB_LangCmd> auto_stmt(m_Conn.LangCmd("COMMIT"));
    auto_stmt->Send();
    auto_stmt->DumpResults();
}


void
CAutoTrans::Rollback(void)
{
    auto_ptr<CDB_LangCmd> auto_stmt(m_Conn.LangCmd("ROLLBACK"));
    auto_stmt->Send();
    auto_stmt->DumpResults();
}


int
CAutoTrans::GetTranCount(void)
{
    int result = 0;
    auto_ptr<CDB_LangCmd> auto_stmt(m_Conn.LangCmd("SELECT @@trancount as tc"));

    if (auto_stmt->Send()) {
        while(auto_stmt->HasMoreResults()) {
            auto_ptr<CDB_Result> rs(auto_stmt->Result());

            if (rs.get() == NULL) {
                continue;
            }

            if (rs->ResultType() != eDB_RowResult) {
                continue;
            }

            if (rs->Fetch()) {
                CDB_Int tran_count;
                rs->GetItem(&tran_count);
                result = tran_count.Value();
            }

            while(rs->Fetch()) {
            }
        }
    }

    return result;
}


END_NCBI_SCOPE


