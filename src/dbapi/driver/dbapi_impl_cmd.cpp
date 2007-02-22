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

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(impl)

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
CBaseCmd::CBaseCmd(const string& query, unsigned int nof_params) :
    m_Query(query),
    m_Params(nof_params),
    m_Recompile(false)
{
}

CBaseCmd::~CBaseCmd(void)
{
    return;
}


CDB_Result* CBaseCmd::Result(void)
{
    return NULL;
}


bool CBaseCmd::HasMoreResults(void) const
{
    return false;
}

void CBaseCmd::DumpResults(void)
{
    return;
}


bool CBaseCmd::Bind(unsigned int column_num, CDB_Object* pVal)
{
    return m_Params.BindParam(column_num, kEmptyStr, pVal);
}


bool CBaseCmd::BindParam(const string& param_name, CDB_Object* param_ptr,
                         bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CBaseCmd::SetParam(const string& param_name, CDB_Object* param_ptr,
                        bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CBaseCmd::More(const string& query_text)
{
    m_Query.append(query_text);
    return true;
}


bool CBaseCmd::CommitBCPTrans(void)
{
    return false;
}


bool CBaseCmd::EndBCP(void)
{
    return false;
}


void CBaseCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CBaseCmd::DetachInterface(void)
{
    m_InterfaceLang.DetachInterface();
    m_InterfaceRPC.DetachInterface();
    m_InterfaceBCPIn.DetachInterface();
}


void CBaseCmd::AttachTo(CDB_LangCmd* interface)
{
    m_InterfaceLang = interface;
}


void CBaseCmd::AttachTo(CDB_RPCCmd* interface)
{
    m_InterfaceRPC = interface;
}

void CBaseCmd::AttachTo(CDB_BCPInCmd* interface)
{
    m_InterfaceBCPIn = interface;
}

///////////////////////////////////////////////////////////////////////////
CCursorCmd::CCursorCmd(const string& cursor_name,
                       const string& query,
                       unsigned int nof_params) :
    m_IsOpen(false),
    m_IsDeclared(false),
    // m_HasFailed(false)
    m_Name(cursor_name),
    m_Query(query),
    m_Params(nof_params)
{
}

CCursorCmd::~CCursorCmd(void)
{
    return;
}

bool CCursorCmd::BindParam(const string& param_name,
                           CDB_Object* param_ptr,
                           bool out_param)
{
    return
        m_Params.BindParam(CDB_Params::kNoParamNumber, param_name, param_ptr,
                           out_param);
}


void CCursorCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CCursorCmd::AttachTo(CDB_CursorCmd* interface)
{
    m_Interface = interface;
}

///////////////////////////////////////////////////////////////////////////
CSendDataCmd::CSendDataCmd(size_t nof_bytes) :
m_Bytes2go(nof_bytes)
{
}

CSendDataCmd::~CSendDataCmd(void)
{
    return;
}

void CSendDataCmd::DetachInterface(void)
{
    m_Interface.DetachInterface();
}

void CSendDataCmd::AttachTo(CDB_SendDataCmd* interface)
{
    m_Interface = interface;
}

END_SCOPE(impl)


END_NCBI_SCOPE


