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
 * File Description:  ODBC cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmdBase
//

CODBC_CursorCmdBase::CODBC_CursorCmdBase(CODBC_Connection* conn,
                                         const string& cursor_name,
                                         const string& query,
                                         unsigned int nof_params) :
    CStatementBase(*conn),
    impl::CCursorCmd(cursor_name, query, nof_params),
    m_CursCmd(conn, query, nof_params)
{
}

CODBC_CursorCmdBase::~CODBC_CursorCmdBase(void)
{
}

bool CODBC_CursorCmdBase::BindParam(const string& param_name, CDB_Object* param_ptr,
                                    bool out_param)
{
    return m_CursCmd.BindParam(param_name, param_ptr, out_param);
}

int CODBC_CursorCmdBase::RowCount(void) const
{
    return static_cast<int>(m_RowCount);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

CODBC_CursorCmd::CODBC_CursorCmd(CODBC_Connection* conn,
                                 const string& cursor_name,
                                 const string& query,
                                 unsigned int nof_params) :
    CODBC_CursorCmdBase(conn, cursor_name, query, nof_params)
{
}

CDB_Result* CODBC_CursorCmd::Open(void)
{
    // need to close it first
    Close();

    m_HasFailed = false;

    // declare the cursor
    try {
        m_CursCmd.SetCursorName(m_Name);
        m_CursCmd.Send();
    } catch (const CDB_Exception& e) {
        string err_message = "failed to declare cursor" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422001 );
    }

    m_IsDeclared = true;

    m_IsOpen = true;

    m_Res.reset(new CODBC_CursorResult(&m_CursCmd));

    return Create_Result(*m_Res);
}


bool CODBC_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!m_IsOpen)
        return false;

    try {
        string buff = upd_query + " where current of " + m_Name;

        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "update failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}

CDB_ITDescriptor* CODBC_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || m_Res.get() == 0) {
        return NULL;
    }

    string cond = "current of " + m_Name;

    return m_CursCmd.m_Res->GetImageOrTextDescriptor(item_num, cond);
}

bool CODBC_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return false;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    return (data.GetType() == eDB_Text)?
        GetConnection().SendData(*desc, (CDB_Text&)data, log_it) :
        GetConnection().SendData(*desc, (CDB_Image&)data, log_it);
}

CDB_SendDataCmd* CODBC_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
                        bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return 0;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    return GetConnection().SendDataCmd((I_ITDescriptor&)*desc, size, log_it);
}

bool CODBC_CursorCmd::Delete(const string& table_name)
{
    if (!m_IsOpen)
        return false;

    try {
        string buff = "delete " + table_name + " where current of " + m_Name;

        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "update failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}


bool CODBC_CursorCmd::Close()
{
    if (!m_IsOpen)
        return false;

    m_Res.reset();

    if (m_IsOpen) {
        m_IsOpen = false;
    }

    if (m_IsDeclared) {
        m_CursCmd.CloseCursor();

        m_IsDeclared = false;
    }

    return true;
}


CODBC_CursorCmd::~CODBC_CursorCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

///////////////////////////////////////////////////////////////////////////////
CODBC_CursorCmdExpl::CODBC_CursorCmdExpl(CODBC_Connection* conn,
                                         const string& cursor_name,
                                         const string& query,
                                         unsigned int nof_params) :
    CODBC_CursorCmd(conn,
                    cursor_name,
                    "declare " + cursor_name + " cursor for " + query,
                    nof_params)
{
}

CODBC_CursorCmdExpl::~CODBC_CursorCmdExpl(void)
{
    DetachInterface();

    GetConnection().DropCmd(*this);

    Close();
}

CDB_Result* CODBC_CursorCmdExpl::Open(void)
{
    // need to close it first
    Close();

    m_HasFailed = false;

    // declare the cursor
    try {
        m_CursCmd.Send();
        m_CursCmd.DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "failed to declare cursor" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422001 );
    }

    m_IsDeclared = true;

    try {
        auto_ptr<impl::CBaseCmd> stmt(GetConnection().xLangCmd("open " + m_Name));

        stmt->Send();
        stmt->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "failed to open cursor" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422002 );
    }

    m_IsOpen = true;

    m_LCmd.reset(GetConnection().xLangCmd("fetch " + m_Name));
    m_Res.reset(new CODBC_CursorResultExpl(m_LCmd.get()));

    return Create_Result(*m_Res);
}


bool CODBC_CursorCmdExpl::Update(const string&, const string& upd_query)
{
    if (!m_IsOpen)
        return false;

    try {
        m_LCmd->Cancel();

        string buff = upd_query + " where current of " + m_Name;

        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "update failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}

CDB_ITDescriptor* CODBC_CursorCmdExpl::x_GetITDescriptor(unsigned int item_num)
{
    if(!m_IsOpen || m_Res.get() == 0 || m_LCmd.get() == 0) {
        return NULL;
    }

    string cond = "current of " + m_Name;

    return m_LCmd->m_Res->GetImageOrTextDescriptor(item_num, cond);
}

bool CODBC_CursorCmdExpl::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return false;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    m_LCmd->Cancel();

    return (data.GetType() == eDB_Text)?
        GetConnection().SendData(*desc, (CDB_Text&)data, log_it) :
        GetConnection().SendData(*desc, (CDB_Image&)data, log_it);
}

CDB_SendDataCmd* CODBC_CursorCmdExpl::SendDataCmd(unsigned int item_num, size_t size,
                        bool log_it)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return 0;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    m_LCmd->Cancel();

    return GetConnection().SendDataCmd((I_ITDescriptor&)*desc, size, log_it);
}

bool CODBC_CursorCmdExpl::Delete(const string& table_name)
{
    if (!m_IsOpen)
        return false;

    try {
        m_LCmd->Cancel();

        string buff = "delete " + table_name + " where current of " + m_Name;

        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "update failed" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}


bool CODBC_CursorCmdExpl::Close()
{
    if (!m_IsOpen)
        return false;

    m_Res.reset();
    m_LCmd.reset();

    if (m_IsOpen) {
        string buff = "close " + m_Name;
        try {
            auto_ptr<CODBC_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "failed to close cursor" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        m_IsOpen = false;
    }

    if (m_IsDeclared) {
        string buff = "deallocate " + m_Name;

        try {
            auto_ptr<CODBC_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "failed to deallocate cursor" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        m_IsDeclared = false;
    }

    return true;
}


END_NCBI_SCOPE


