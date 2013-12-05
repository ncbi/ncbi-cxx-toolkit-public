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
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Cmds

#undef NCBI_DATABASE_THROW
#undef NCBI_DATABASE_RETHROW

#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_ODBC_THROW(ex_class, message, err_code, severity)
#define NCBI_DATABASE_RETHROW(prev_ex, ex_class, message, err_code, severity) \
    NCBI_ODBC_RETHROW(prev_ex, ex_class, message, err_code, severity)


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmdBase
//

CODBC_CursorCmdBase::CODBC_CursorCmdBase(CODBC_Connection& conn,
                                         const string& cursor_name,
                                         const string& query)
: CStatementBase(conn, cursor_name, query)
, m_CursCmd(conn, query)
{
    SetDbgInfo("Cursor Name: \"" + cursor_name + "\"; SQL Command: \""
               + query + "\"");
}

CODBC_CursorCmdBase::~CODBC_CursorCmdBase(void)
{
}

CDBParams& CODBC_CursorCmdBase::GetBindParams(void)
{
    return m_CursCmd.GetBindParams();
}

CDBParams& CODBC_CursorCmdBase::GetDefineParams(void)
{
    return m_CursCmd.GetDefineParams();
}

int CODBC_CursorCmdBase::RowCount(void) const
{
    return static_cast<int>(m_RowCount);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

CODBC_CursorCmd::CODBC_CursorCmd(CODBC_Connection& conn,
                                 const string& cursor_name,
                                 const string& query)
: CODBC_CursorCmdBase(conn, cursor_name, query)
{
}

CDB_Result* CODBC_CursorCmd::OpenCursor(void)
{
    // need to close it first
    CloseCursor();

    SetHasFailed(false);

    // declare the cursor
    try {
        m_CursCmd.SetCursorName(GetCmdName());
        m_CursCmd.Send();
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to declare cursor." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422001 );
    }

    SetCursorDeclared();

    SetCursorOpen();
    GetBindParamsImpl().LockBinding();

    m_Res.reset(new CODBC_CursorResult(&m_CursCmd));

    return Create_Result(*m_Res);
}


bool CODBC_CursorCmd::Update(const string&, const string& upd_query)
{
    if (!CursorIsOpen())
        return false;

    try {
        string buff = upd_query + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}

CDB_ITDescriptor* CODBC_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || m_Res.get() == 0) {
        return NULL;
    }

    string cond = "current of " + GetCmdName();

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
                                              bool log_it,
                                              bool dump_results)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return 0;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    return GetConnection().SendDataCmd((I_ITDescriptor&)*desc, size, log_it, dump_results);
}

bool CODBC_CursorCmd::Delete(const string& table_name)
{
    if (!CursorIsOpen())
        return false;

    try {
        string buff = "delete " + table_name + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}


bool CODBC_CursorCmd::CloseCursor()
{
    if (!CursorIsOpen())
        return false;

    m_Res.reset();

    if (CursorIsOpen()) {
        SetCursorOpen(false);
    }

    if (CursorIsDeclared()) {
        m_CursCmd.CloseCursor();

        SetCursorDeclared(false);
    }

    return true;
}


CODBC_CursorCmd::~CODBC_CursorCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        CloseCursor();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}

///////////////////////////////////////////////////////////////////////////////
CODBC_CursorCmdExpl::CODBC_CursorCmdExpl(CODBC_Connection& conn,
                                         const string& cursor_name,
                                         const string& query) :
    CODBC_CursorCmd(conn,
                    cursor_name,
                    "declare " + cursor_name + " cursor for " + query)
{
}

CODBC_CursorCmdExpl::~CODBC_CursorCmdExpl(void)
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        CloseCursor();
    }
    NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )
}

CDB_Result* CODBC_CursorCmdExpl::OpenCursor(void)
{
    // need to close it first
    CloseCursor();

    SetHasFailed(false);

    // declare the cursor
    try {
        m_CursCmd.Send();
        m_CursCmd.DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to declare cursor." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422001 );
    }

    SetCursorDeclared();

    try {
        auto_ptr<impl::CBaseCmd> stmt(GetConnection().xLangCmd("open " + GetCmdName()));

        stmt->Send();
        stmt->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to open cursor." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422002 );
    }

    SetCursorOpen();
    GetBindParamsImpl().LockBinding();

    m_LCmd.reset(GetConnection().xLangCmd("fetch " + GetCmdName()));
    m_Res.reset(new CODBC_CursorResultExpl(m_LCmd.get()));

    return Create_Result(*m_Res);
}


bool CODBC_CursorCmdExpl::Update(const string&, const string& upd_query)
{
    if (!CursorIsOpen())
        return false;

    try {
        m_LCmd->Cancel();

        string buff = upd_query + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}

CDB_ITDescriptor* CODBC_CursorCmdExpl::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || m_Res.get() == 0 || m_LCmd.get() == 0) {
        return NULL;
    }

    string cond = "current of " + GetCmdName();

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
                                                  bool log_it,
                                                  bool dump_results)
{
    CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    if(desc == 0) return 0;
    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    m_LCmd->Cancel();

    return GetConnection().SendDataCmd((I_ITDescriptor&)*desc, size, log_it, dump_results);
}

bool CODBC_CursorCmdExpl::Delete(const string& table_name)
{
    if (!CursorIsOpen())
        return false;

    try {
        m_LCmd->Cancel();

        string buff = "delete " + table_name + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}


bool CODBC_CursorCmdExpl::CloseCursor()
{
    if (!CursorIsOpen())
        return false;

    m_Res.reset();
    m_LCmd.reset();

    if (CursorIsOpen()) {
        string buff = "close " + GetCmdName();
        try {
            auto_ptr<CODBC_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "Failed to close cursor." + GetDbgInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        SetCursorOpen(false);
    }

    if (CursorIsDeclared()) {
        string buff = "deallocate " + GetCmdName();

        try {
            auto_ptr<CODBC_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "Failed to deallocate cursor." + GetDbgInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        SetCursorDeclared(false);
    }

    return true;
}


END_NCBI_SCOPE


