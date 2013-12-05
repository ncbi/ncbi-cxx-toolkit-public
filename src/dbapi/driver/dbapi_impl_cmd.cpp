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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>

BEGIN_NCBI_SCOPE

namespace impl
{

///////////////////////////////////////////////////////////////////////////
CCommand::~CCommand(void)
{
}


CDB_Result*
CCommand::Create_Result(CResult& result)
{
    return new CDB_Result(&result);
}


///////////////////////////////////////////////////////////////////////////
CCmdBase::CCmdBase(impl::CConnection& conn)
: m_ConnImpl(&conn)
, m_WasSent(false)
{
    _ASSERT(m_ConnImpl);
}


CCmdBase::~CCmdBase()
{
}


///////////////////////////////////////////////////////////////////////////
CBaseCmd::CBaseCmd(impl::CConnection& conn,
                   const string& query)
: CCmdBase(conn)
, m_Query(query)
, m_InParams(GetBindParamsImpl())
, m_OutParams(GetDefineParamsImpl())
, m_Recompile(false)
, m_HasFailed(false)
, m_IsOpen(false)
, m_IsDeclared(false)
, m_RowsSent(0)
, m_BatchesSent(0)
{
}

CBaseCmd::CBaseCmd(impl::CConnection& conn,
                   const string& cursor_name,
                   const string& query)
: CCmdBase(conn)
, m_Query(query)
, m_InParams(GetBindParamsImpl())
, m_OutParams(GetDefineParamsImpl())
, m_Recompile(false)
, m_HasFailed(false)
, m_IsOpen(false)
, m_IsDeclared(false)
, m_CmdName(cursor_name)
, m_RowsSent(0)
, m_BatchesSent(0)
{
}

CBaseCmd::~CBaseCmd(void)
{
    return;
}


CDBParams& CBaseCmd::GetBindParams(void)
{
    return m_InParams;
}

CDBParams& CBaseCmd::GetDefineParams(void)
{
    return m_OutParams;
}

bool
CBaseCmd::Send(void)
{
    _ASSERT(false);
    return false;
}


bool
CBaseCmd::Cancel(void)
{
    _ASSERT(false);
    return false;
}


bool
CBaseCmd::HasFailed(void) const
{
    return m_HasFailed;
}


CDB_Result*
CBaseCmd::Result(void)
{
    return NULL;
}


bool
CBaseCmd::HasMoreResults(void) const
{
    return false;
}

void
CBaseCmd::DumpResults(void)
{
    // Experimental ...
    while(HasMoreResults()) {
        auto_ptr<CDB_Result> dbres(Result());

        if( dbres.get() ) {
            CDB_ResultProcessor* res_proc = GetConnImpl().GetResultProcessor();
            if(res_proc) {
                res_proc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch()) {
                    continue;
                }
            }
        }
    }
}


void
CBaseCmd::SetHints(CTempString hints)
{
    _ASSERT(false);
}


void
CBaseCmd::AddHint(CDB_BCPInCmd::EBCP_Hints hint, unsigned int value)
{
    _ASSERT(false);
}


void
CBaseCmd::AddOrderHint(CTempString columns)
{
    _ASSERT(false);
}


bool
CBaseCmd::CommitBCPTrans(void)
{
    return false;
}


bool
CBaseCmd::EndBCP(void)
{
    return false;
}


CDB_Result*
CBaseCmd::OpenCursor(void)
{
    _ASSERT(false);
    return NULL;
}

bool
CBaseCmd::Update(const string& /* table_name */, const string& /* upd_query */)
{
    _ASSERT(false);
    return false;
}


bool
CBaseCmd::UpdateTextImage(unsigned int /* item_num */,
                          CDB_Stream& /* data */,
                          bool /* log_it */
                          )
{
    _ASSERT(false);
    return false;
}


CDB_SendDataCmd*
CBaseCmd::SendDataCmd(unsigned int /* item_num */,
                      size_t /* size */,
                      bool /* log_it */,
                      bool /* dump_results */
                      )
{
    _ASSERT(false);
    return NULL;
}


bool
CBaseCmd::Delete(const string& /* table_name */)
{
    _ASSERT(false);
    return false;
}


bool
CBaseCmd::CloseCursor(void)
{
    _ASSERT(false);
    return false;
}


void
CBaseCmd::DetachInterface(void)
{
    m_InterfaceLang.DetachInterface();
    m_InterfaceRPC.DetachInterface();
    m_InterfaceBCPIn.DetachInterface();
    m_InterfaceCursor.DetachInterface();
}


void
CBaseCmd::AttachTo(CDB_LangCmd* interface)
{
    m_InterfaceLang = interface;
}


void
CBaseCmd::AttachTo(CDB_RPCCmd* interface)
{
    m_InterfaceRPC = interface;
}


void
CBaseCmd::AttachTo(CDB_BCPInCmd* interface)
{
    m_InterfaceBCPIn = interface;
}


void CBaseCmd::AttachTo(CDB_CursorCmd* interface)
{
    m_InterfaceCursor = interface;
}


///////////////////////////////////////////////////////////////////////////
CSendDataCmd::CSendDataCmd(impl::CConnection& conn,
                           size_t             nof_bytes)
: CCmdBase(conn)
, m_Bytes2Go(nof_bytes)
{
}

CSendDataCmd::~CSendDataCmd(void)
{
    return;
}

void
CSendDataCmd::DetachSendDataIntf(void)
{
    m_Interface.DetachInterface();
}

void
CSendDataCmd::AttachTo(CDB_SendDataCmd* interface)
{
    m_Interface = interface;
}

CDB_Result*
CSendDataCmd::Result(void)
{
    return NULL;
}


bool
CSendDataCmd::HasMoreResults(void) const
{
    return false;
}

void
CSendDataCmd::DumpResults(void)
{
    // Experimental ...
    while(HasMoreResults()) {
        auto_ptr<CDB_Result> dbres(Result());

        if( dbres.get() ) {
            CDB_ResultProcessor* res_proc = GetConnImpl().GetResultProcessor();
            if(res_proc) {
                res_proc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch()) {
                    continue;
                }
            }
        }
    }
}

} // namespace impl

END_NCBI_SCOPE


