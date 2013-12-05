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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*
* File Description:  Statement implementation
*
*
*
*
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "stmt_impl.hpp"
#include "rs_impl.hpp"
#include "rw_impl.hpp"
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
CStatement::CStmtParamsMetaData::CStmtParamsMetaData(I_BaseCmd*& cmd)
: m_Cmd(cmd)
{
}

CStatement::CStmtParamsMetaData::~CStmtParamsMetaData()
{
}


unsigned int CStatement::CStmtParamsMetaData::GetTotalColumns() const
{
    _ASSERT(m_Cmd);
    return m_Cmd->GetBindParams().GetNum();
}

EDB_Type CStatement::CStmtParamsMetaData::GetType(const CDBParamVariant& param) const
{
    _ASSERT(m_Cmd);
    return m_Cmd->GetBindParams().GetDataType(param);
}

int CStatement::CStmtParamsMetaData::GetMaxSize(const CDBParamVariant& param) const
{
    _ASSERT(m_Cmd);
    return m_Cmd->GetBindParams().GetDataType(param);
}

string CStatement::CStmtParamsMetaData::GetName(const CDBParamVariant& param) const
{
    _ASSERT(m_Cmd);
    return m_Cmd->GetBindParams().GetName(param);
}

CDBParams::EDirection CStatement::CStmtParamsMetaData::GetDirection(const CDBParamVariant& param) const
{
    _ASSERT(m_Cmd);
    return m_Cmd->GetBindParams().GetDirection(param);
}

////////////////////////////////////////////////////////////////////////////////
// implementation
CStatement::CStatement(CConnection* conn)
: m_conn(conn)
, m_cmd(NULL)
, m_InParams(m_cmd)
, m_rowCount(-1)
, m_failed(false)
, m_irs(0)
, m_wr(0)
, m_ostr(0)
, m_AutoClearInParams(false)
{
    SetIdent("CStatement");
}

CStatement::~CStatement()
{
    try {
        Notify(CDbapiClosedEvent(this));
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted.");
    }
    NCBI_CATCH_ALL_X( 9, kEmptyStr )
}

IConnection* CStatement::GetParentConn()
{
    return m_conn;
}

void CStatement::CacheResultSet(CDB_Result *rs)
{
    if( m_irs != 0 ) {
        _TRACE("CStatement::CacheResultSet(): Invalidating cached CResultSet " << (void*)m_irs);
        m_irs->Invalidate();
    }

    if( rs != 0 ) {
        m_irs = new CResultSet(m_conn, rs);
        m_irs->AddListener(this);
        AddListener(m_irs);
        _TRACE("CStatement::CacheResultSet(): Created new CResultSet " << (void*)m_irs
            << " with CDB_Result " << (void*)rs);
    } else {
        m_irs = 0;
    }
}

IResultSet* CStatement::GetResultSet()
{
   return m_irs;
}

bool CStatement::HasMoreResults()
{
    // This method may be called even before *execute*.
    // We have to be prepared for everything.
    bool more = (GetBaseCmd() != NULL);

    if (more) {
        more = GetBaseCmd()->HasMoreResults();
        if( more ) {
            if( GetBaseCmd()->HasFailed() ) {
                SetFailed(true);
                return false;
            }
            //Notify(CDbapiNewResultEvent(this));
            CDB_Result *rs = GetBaseCmd()->Result();
            CacheResultSet(rs);
#if 0
            if( rs == 0 ) {
                m_rowCount = GetBaseCmd()->RowCount();
            }
#endif
        }
    }

    return more;
}

void CStatement::SetParam(const CVariant& v,
                          const CDBParamVariant& param)
{
    if (param.IsPositional()) {
        if (!m_params.empty()) {
            NCBI_DBAPI_THROW("Binding by position is prohibited if any parameter was bound by name.");
        }
        if (m_posParams.size() < param.GetPosition())
            m_posParams.resize(param.GetPosition());
        CVariant*& var = m_posParams[param.GetPosition() - 1];
        if (var)
            *var = v;
        else
            var = new CVariant(v);
    } else {
        if (!m_posParams.empty()) {
            NCBI_DBAPI_THROW("Binding by name is prohibited if any parameter was bound by position.");
        }
        const string name = param.GetName();
        ParamList::iterator i = m_params.find(name);
        if( i != m_params.end() ) {
            *((*i).second) = v;
        }
        else {
            m_params.insert(make_pair(name, new CVariant(v)));
        }
    }
}


void CStatement::ClearParamList()
{
    for(ParamList::iterator i = m_params.begin(); i != m_params.end(); ++i ) {
        delete (*i).second;
    }
    for(ParamByPosList::iterator i = m_posParams.begin(); i != m_posParams.end(); ++i ) {
        delete (*i);
    }

    m_params.clear();
    m_posParams.clear();
}

void CStatement::Execute(const string& sql)
{
    x_Send(sql);
}

void CStatement::SendSql(const string& sql)
{
    x_Send(sql);
}

void CStatement::x_Send(const string& sql)
{
    if( m_cmd != 0 ) {
        delete m_cmd;
        m_cmd = 0;
        m_rowCount = -1;
    }

    SetFailed(false);

    m_cmd = m_conn->GetCDB_Connection()->LangCmd(sql);

    ExecuteLast();

    if ( IsAutoClearInParams() ) {
        // Implicitely clear all parameters.
        ClearParamList();
    }
}

IResultSet* CStatement::ExecuteQuery(const string& sql)
{
    SendSql(sql);

    while ( HasMoreResults() ) {
        if ( HasRows() ) {
            return GetResultSet();
        }
    }

    return 0;
}
void CStatement::ExecuteUpdate(const string& sql)
{
    SendSql(sql);

    PurgeResults();
}

void CStatement::ExecuteLast()
{
    for(ParamList::iterator i = m_params.begin(); i != m_params.end(); ++i ) {
        GetLangCmd()->GetBindParams().Bind((*i).first, (*i).second->GetData());
    }
    for(unsigned int i = 0; i < m_posParams.size(); ++i) {
        CVariant* var = m_posParams[i];
        if (!var) {
            NCBI_DBAPI_THROW("Not all parameters were bound by position.");
        }
        GetLangCmd()->GetBindParams().Bind(i, var->GetData());
    }
    m_cmd->Send();
}

const IResultSetMetaData&
CStatement::GetParamsMetaData(void)
{
    return m_InParams;
}

bool CStatement::HasRows()
{
    return m_irs != 0;
}

IWriter* CStatement::GetBlobWriter(I_ITDescriptor &d, size_t blob_size, EAllowLog log_it)
{
    delete m_wr;
    m_wr = 0;
    m_wr = new CxBlobWriter(GetConnection()->GetCDB_Connection(),
                            d, blob_size, log_it == eEnableLog, false);
    return m_wr;
}

CNcbiOstream& CStatement::GetBlobOStream(I_ITDescriptor &d, size_t blob_size,
                                         EAllowLog log_it, size_t buf_size)
{
    delete m_ostr;
    m_ostr = 0;
    m_ostr = new CWStream(new CxBlobWriter(GetConnection()->GetCDB_Connection(),
                                           d, blob_size, log_it == eEnableLog,
                                           false),
                          buf_size, 0, (CRWStreambuf::fOwnWriter |
                                        CRWStreambuf::fLogExceptions));
    return *m_ostr;
}

CDB_Result* CStatement::GetCDB_Result()
{
    return m_irs == 0 ? 0 : m_irs->GetCDB_Result();
}

bool CStatement::Failed()
{
    return m_failed;
}

int CStatement::GetRowCount()
{
    int v;

    if( (v = GetBaseCmd()->RowCount()) >= 0 ) {
        m_rowCount = v;
    }

    return m_rowCount;
}

void CStatement::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CStatement::FreeResources()
{
    delete m_cmd;
    m_cmd = 0;
    m_rowCount = -1;

    if ( m_conn != 0 && m_conn->IsAux() ) {
        delete m_conn;
        m_conn = 0;
        Notify(CDbapiAuxDeletedEvent(this));
    }

    delete m_wr;
    m_wr = 0;
    delete m_ostr;
    m_ostr = 0;

    ClearParamList();
}

void CStatement::PurgeResults()
{
    while (HasMoreResults())
    {
        if (HasRows()) {
            auto_ptr<IResultSet> rs( GetResultSet() );
            if (rs.get()) {
                // Is it necessary???
                while (rs->Next()) {
                    ;
                }
            }
        }
    }
}

void CStatement::Cancel()
{
    if( GetBaseCmd() != 0 )
        GetBaseCmd()->Cancel();

    m_rowCount = -1;
}

CDB_LangCmd* CStatement::GetLangCmd()
{
    //if( m_cmd == 0 )
    //throw CDbException("CStatementImpl::GetLangCmd(): no cmd structure");
    return (CDB_LangCmd*)m_cmd;
}

void CStatement::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName()
           << "' received from " << e.GetSource()->GetIdent());

    CResultSet *rs;

    if (dynamic_cast<const CDbapiFetchCompletedEvent*>(&e) != 0 ) {
        if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                m_rowCount = rs->GetTotalRows();
                _TRACE("Rowcount from the last resultset: " << m_rowCount);
            }
        }
    }

    if (dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this);
            delete this;
        }
        else if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                _TRACE("Clearing cached CResultSet " << (void*)m_irs);
                m_irs = 0;
            }
        }
    }
}

END_NCBI_SCOPE
