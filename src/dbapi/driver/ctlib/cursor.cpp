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
 * File Description:  CTLib cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/error_codes.hpp>

#include <stdio.h>


#define NCBI_USE_ERRCODE_X   Dbapi_CTlib_Cmds

#undef NCBI_DATABASE_THROW
#undef NCBI_DATABASE_RETHROW

#define NCBI_DATABASE_THROW(ex_class, message, err_code, severity) \
    NCBI_DATABASE_THROW_ANNOTATED(ex_class, message, err_code, severity, \
        GetDbgInfo(), GetConnection(), &GetBindParams())
#define NCBI_DATABASE_RETHROW(prev_ex, ex_class, message, err_code, severity) \
    NCBI_DATABASE_RETHROW_ANNOTATED(prev_ex, ex_class, message, err_code, \
        severity, GetDbgInfo(), GetConnection(), &GetBindParams())

BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace ftds64_ctlib
{
#endif


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

CTL_CursorCmd::CTL_CursorCmd(CTL_Connection& conn,
                             const string& cursor_name,
                             const string& query,
                             unsigned int fetch_size
                             )
: CTL_Cmd(conn, cursor_name, query)
, m_FetchSize(fetch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExecCntxInfo(extra_msg);
}


CS_RETCODE
CTL_CursorCmd::CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    try {
        rc = Check(rc);
    } catch (...) {
        SetHasFailed();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}


CS_RETCODE
CTL_CursorCmd::CheckSFBCP(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    try {
        rc = Check(rc);
    } catch (...) {
        SetHasFailed();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 122008 );
    case CS_PENDING:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 122007 );
    }

    return rc;
}


bool
CTL_CursorCmd::ProcessResults(void)
{
    // process the results
    for (;;) {
        CS_INT res_type;

        if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                       "ct_result failed", 122045) == CS_END_RESULTS) {
            return true;
        }

        if (ProcessResultInternal(res_type)) {
            continue;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            continue;
        case CS_CMD_FAIL: // the command has failed
            SetHasFailed();
            while(Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED)
            {
                continue;
            }
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122049 );
        default:
            continue;
        }
    }

    return false;
}


CDB_Result*
CTL_CursorCmd::OpenCursor()
{
    // need to close it first
    CloseCursor();

    CheckIsDead();

    if (!CursorIsDeclared()) {
        SetHasFailed(false);

        CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DECLARE,
                           const_cast<char*>(GetCmdName().data()),
                           GetCmdName().size(),
                           const_cast<char*>(GetQuery().data()),
                           GetQuery().size(),
                           CS_UNUSED),
                 "ct_cursor(DECLARE) failed", 122001);

        if (GetBindParamsImpl().NofParams() > 0) {
            // we do have the parameters
            // check if query is a select statement or a function call
            if (GetQuery().find("select") != string::npos  ||
                GetQuery().find("SELECT") != string::npos) {
                // this is a select
                SetHasFailed(!x_AssignParams(true));
                CHECK_DRIVER_ERROR( HasFailed(), "Cannot declare the params." +
                                    GetDbgInfo(), 122003 );
            }
        }

        if (m_FetchSize > 1) {
            CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_ROWS, 0, CS_UNUSED,
                               0, CS_UNUSED, (CS_INT) m_FetchSize),
                     "ct_cursor(ROWS) failed", 122004);
        }

        // Execute command ...
        // Current inplementation of FreeTDS ctlib won't send CS_CURSOR_DECLARE
        // command. So, error processing can be done only after CS_CURSOR_OPEN
        // was sent.
        {
            // send this command
            CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122006);

            // Check results of send ...
            ProcessResults();
        }

        SetCursorDeclared();
    }

    SetHasFailed(false);

    // open cursor
    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_OPEN, 0, CS_UNUSED, 0, CS_UNUSED,
                       CursorIsDeclared() ? CS_RESTORE_OPEN : CS_UNUSED),
             "ct_cursor(open) failed", 122005);

    if (GetBindParamsImpl().NofParams() > 0) {
        // we do have parameters
        SetHasFailed(!x_AssignParams(false));
        CHECK_DRIVER_ERROR( HasFailed(), "Cannot assign the params." + GetDbgInfo(), 122003 );
    }

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122006);

    // Process results ....
    for (;;) {
        CS_INT res_type;

        try {
            if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                        "ct_result failed", 122013) == CS_END_RESULTS) {
                return NULL;
            }
        } catch (...) {
            // We have to fech out all pending  results ...
            while (ct_results(x_GetSybaseCmd(), &res_type) == CS_SUCCEED) {
                continue;
            }
            throw;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            // done with this command -- check the number of affected rows
            GetRowCount(&m_RowCount);

            continue;
        case CS_CMD_FAIL:
            // the command has failed -- check the number of affected rows
            GetRowCount(&m_RowCount);
            SetHasFailed();
            while (Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED) {
                continue;
            }
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122016 );
        case CS_CURSOR_RESULT:
            // Cursor can be set open only after processing of ct_send, which does
            // actual job.
            SetCursorOpen();

            SetResult(MakeCursorResult());
            break;
        default:
            continue;
        }

        return Create_Result(static_cast<impl::CResult&>(GetResult()));
    }
}


bool CTL_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    if (!CursorIsOpen()) {
        return false;
    }

    CheckIsDead();

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_UPDATE,
                       const_cast<char*>(table_name.data()), table_name.size(),
                       const_cast<char*>(upd_query.data()),  upd_query.size(),
                       CS_UNUSED),
             "ct_cursor(update) failed", 122030);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122032);

    // process results
    return ProcessResults();
}

I_ITDescriptor* CTL_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || !HaveResult()) {
        return 0;
    }

    CheckIsDead();

    while ( static_cast<unsigned int>(GetResult().CurrentItemNo()) < item_num ) {
        if(!GetResult().SkipItem()) return 0;
    }

    I_ITDescriptor* desc= 0;
    if(static_cast<unsigned int>(GetResult().CurrentItemNo()) == item_num) {
        desc = GetResult().GetImageOrTextDescriptor();
    }
    else {
        auto_ptr<CTL_ITDescriptor> dsc(new CTL_ITDescriptor);

        bool rc = (Check(ct_data_info(x_GetSybaseCmd(), CS_GET, item_num+1, &dsc->m_Desc))
                   != CS_SUCCEED);

        CHECK_DRIVER_ERROR(
            rc,
            "ct_data_info failed." + GetDbgInfo(),
            130010 );
        desc = dsc.release();
    }
    return desc;
}

bool CTL_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? x_SendData(*desc, data, log_it) : false;
}

CDB_SendDataCmd* CTL_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
                                            bool log_it,
                                            bool dump_results)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? ConnSendDataCmd(*desc, size, log_it, dump_results) : 0;
}

bool CTL_CursorCmd::Delete(const string& table_name)
{
    if (!CursorIsOpen()) {
        return false;
    }

    CheckIsDead();

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DELETE,
                       const_cast<char*>(table_name.data()), table_name.size(),
                       0, CS_UNUSED, CS_UNUSED),
             "ct_cursor(delete) failed", 122040);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122042);

    // process the results
    return ProcessResults();
}


int CTL_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmd::CloseCursor(void)
{
    if (!CursorIsOpen()) {
        return false;
    }

    DeleteResult();

    if (IsDead()) {
        SetCursorOpen(false);
        return true;
    }

    CheckSFB(ct_cursor(x_GetSybaseCmd(),
                       CS_CURSOR_CLOSE,
                       0,
                       CS_UNUSED,
                       0,
                       CS_UNUSED,
                       CS_UNUSED),
             "ct_cursor(close) failed", 122020);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122022);

    // Process results ...
    bool result = ProcessResults();

    // An exception can be thrown in ProcessResults, so, we set the flag after
    // calling of ProcessResults.
    SetCursorOpen(!result);

    return result;
}


void
CTL_CursorCmd::CloseForever(void)
{
    if (x_GetSybaseCmd()) {

        // ????
        DetachInterface();

        CloseCursor();

        if (CursorIsDeclared()  &&  !IsDead()) {
            // deallocate the cursor
            switch ( Check(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DEALLOC,
                               0, CS_UNUSED, 0, CS_UNUSED, CS_UNUSED)) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // SetHasFailed();
                //throw CDB_ClientEx(eDiag_Fatal, 122050, "::~CTL_CursorCmd",
                //                   "ct_cursor(dealloc) failed");
#ifdef CS_BUSY
            case CS_BUSY:
                //throw CDB_ClientEx(eDiag_Error, 122051, "::~CTL_CursorCmd",
                //                   "the connection is busy");
#endif
                DropSybaseCmd();
                return;
            }

            // send this command
            switch ( Check(ct_send(x_GetSybaseCmd())) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // SetHasFailed();
                // throw CDB_ClientEx(eDiag_Error, 122052, "::~CTL_CursorCmd",
                //                   "ct_send failed");
            case CS_CANCELED:
                // throw CDB_ClientEx(eDiag_Error, 122053, "::~CTL_CursorCmd",
                //                   "command was canceled");
#ifdef CS_BUSY
            case CS_BUSY:
#endif
            case CS_PENDING:
                // throw CDB_ClientEx(eDiag_Error, 122054, "::~CTL_CursorCmd",
                //                   "connection has another request pending");
                DropSybaseCmd();
                return;
            }

            // process the results
            try {
                ProcessResults();
            } catch(CDB_ClientEx const&)
            {
                // Just ignore ...
                _ASSERT(false);
            }

        }

        DropSybaseCmd();
    }
}


CTL_CursorCmd::~CTL_CursorCmd()
{
    try {
        DetachInterface();

        DropCmd(*this);

        CloseForever();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


bool CTL_CursorCmd::x_AssignParams(bool declare_only)
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < GetBindParamsImpl().NofParams();  i++) {
        if (GetBindParamsImpl().GetParamStatus(i) == 0) {
            continue;
        }

        CDB_Object& param = *GetBindParamsImpl().GetParam(i);
        const string& param_name = GetBindParamsImpl().GetParamName(i);

        if ( !AssignCmdParam(param, param_name, param_fmt, declare_only) ) 
        {
            return false;
        }
    }

    GetBindParamsImpl().LockBinding();
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmdExpl::
//

CTL_CursorCmdExpl::CTL_CursorCmdExpl(CTL_Connection& conn,
                                     const string& cursor_name,
                                     const string& query,
                                     unsigned int fetch_size)
    : CTL_Cmd(conn, cursor_name, query),
      m_LCmd(NULL),
      m_Res(NULL)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExecCntxInfo(extra_msg);
}


static bool for_update_of(const string& q)
{
    if((q.find("update") == string::npos) &&
       (q.find("UPDATE") == string::npos))
        return false;

    if((q.find("for update") != string::npos) ||
       (q.find("FOR UPDATE") != string::npos))
        return true;

    // TODO: add more logic here to find "for update" clause
    return false;
}

CDB_Result* CTL_CursorCmdExpl::OpenCursor()
{
    const bool connected_to_MSSQLServer = 
        GetConnection().GetServerType() == CDBConnParams::eMSSqlServer;

    // need to close it first
    CloseCursor();

    SetHasFailed(false);

    // declare the cursor
    SetHasFailed(!x_AssignParams());
    CHECK_DRIVER_ERROR(
        HasFailed(),
        "Cannot assign params." + GetDbgInfo(),
        122503 );


    m_LCmd.reset(0);

    string buff;
    if ( connected_to_MSSQLServer ) {
        string cur_feat;

        if(for_update_of(GetCombinedQuery())) {
            cur_feat = " cursor FORWARD_ONLY SCROLL_LOCKS for ";
        } else {
            cur_feat = " cursor FORWARD_ONLY for ";
        }

        buff = "declare " + GetCmdName() + cur_feat + GetCombinedQuery();
    } else {
        // Sybase ...

        buff = "declare " + GetCmdName() + " cursor for " + GetCombinedQuery();
    }

    try {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to declare cursor." + GetDbgInfo(), 122501 );
    }

    SetCursorDeclared();

    // open the cursor
    buff = "open " + GetCmdName();

    try {
        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));

        cmd->Send();
        cmd->DumpResults();
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to open cursor." + GetDbgInfo(), 122502 );
    }

    SetCursorOpen();

    buff = "fetch " + GetCmdName();
    m_LCmd.reset(GetConnection().xLangCmd(buff));
    m_Res.reset(new CTL_CursorResultExpl(m_LCmd.get()));

    return Create_Result(*GetResultSet());
}


bool CTL_CursorCmdExpl::Update(const string&, const string& upd_query)
{
    if (!CursorIsOpen())
        return false;

    try {
        while(m_LCmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(m_LCmd->Result());
        }

        string buff = upd_query + " where current of " + GetCmdName();
        const auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
#if 0
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
#endif
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Update failed." + GetDbgInfo(), 122507 );
    }

    return true;
}

I_ITDescriptor* CTL_CursorCmdExpl::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || !m_Res.get() || !m_LCmd.get()) {
        return 0;
    }
    CheckIsDead();
    while(static_cast<unsigned int>(m_Res->CurrentItemNo()) < item_num) {
        if(!m_Res->SkipItem()) return 0;
    }

    I_ITDescriptor* desc = m_Res->GetImageOrTextDescriptor(item_num);
    return desc;
}

bool CTL_CursorCmdExpl::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    if(desc) {
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }

        return GetConnection().x_SendData(*desc, data, log_it);
    }
    return false;
}

CDB_SendDataCmd* CTL_CursorCmdExpl::SendDataCmd(unsigned int item_num, size_t size,
                                                bool log_it,
                                                bool dump_results)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    if(desc) {
        m_LCmd->DumpResults();
#if 0
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }
#endif

        return GetConnection().SendDataCmd(*desc, size, log_it, dump_results);
    }
    return 0;
}

bool CTL_CursorCmdExpl::Delete(const string& table_name)
{
    if (!CursorIsOpen())
        return false;

    CDB_LangCmd* cmd = 0;

    try {
        while(m_LCmd->HasMoreResults()) {
            CDB_Result* r= m_LCmd->Result();
            if(r) delete r;
        }

        string buff = "delete " + table_name + " where current of " + GetCmdName();
        cmd = GetConnection().LangCmd(buff);
        cmd->Send();
        cmd->DumpResults();
#if 0
        while (cmd->HasMoreResults()) {
            CDB_Result* r = cmd->Result();
            if (r) {
                while (r->Fetch())
                    ;
                delete r;
            }
        }
#endif
        delete cmd;
    } catch ( const CDB_Exception& e ) {
        if (cmd)
            delete cmd;
        DATABASE_DRIVER_ERROR_EX( e, "Update failed." + GetDbgInfo(), 122506 );
    }

    return true;
}


int CTL_CursorCmdExpl::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmdExpl::CloseCursor()
{
    if (!CursorIsOpen())
        return false;

    m_Res.reset(0);

    m_LCmd.reset(0);

    if (CursorIsOpen()) {
        string buff = "close " + GetCmdName();
        try {
            m_LCmd.reset(GetConnection().xLangCmd(buff));
            m_LCmd->Send();
            m_LCmd->DumpResults();
#if 0
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
#endif
            m_LCmd.reset(0);
        } catch ( const CDB_Exception& e ) {
            m_LCmd.reset(0);
            DATABASE_DRIVER_ERROR_EX( e, "Failed to close cursor." + GetDbgInfo(), 122504 );
        }

        SetCursorOpen(false);
    }

    if (CursorIsDeclared()) {
        string buff;
        if (GetConnection().GetServerType() == CDBConnParams::eMSSqlServer)
            buff = "deallocate ";
        else
            buff = "deallocate cursor ";
        buff += GetCmdName();

        try {
            m_LCmd.reset(GetConnection().xLangCmd(buff));
            m_LCmd->Send();
            m_LCmd->DumpResults();
#if 0
            while (m_LCmd->HasMoreResults()) {
                CDB_Result* r = m_LCmd->Result();
                if (r) {
                    while (r->Fetch())
                        ;
                    delete r;
                }
            }
#endif
            m_LCmd.reset(0);
        } catch ( const CDB_Exception& e) {
            m_LCmd.reset(0);
            DATABASE_DRIVER_ERROR_EX( e, "Failed to deallocate cursor." + GetDbgInfo(), 122505 );
        }

        SetCursorDeclared(false);
    }

    return true;
}


CTL_CursorCmdExpl::~CTL_CursorCmdExpl()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        CloseCursor();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


bool CTL_CursorCmdExpl::x_AssignParams()
{
    static const char s_hexnum[] = "0123456789ABCDEF";

    m_CombinedQuery = GetQuery();

    for (unsigned int n = 0; n < GetBindParamsImpl().NofParams(); n++) {
        const string& name = GetBindParamsImpl().GetParamName(n);
        if (name.empty())
            continue;
        CDB_Object& param = *GetBindParamsImpl().GetParam(n);
        char val_buffer[16*1024];

        if (!param.IsNULL()) {
            switch (param.GetType()) {
            case eDB_Bit: 
                DATABASE_DRIVER_ERROR("Bit data type is not supported", 10005);
                break;
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                sprintf(val_buffer, "%d", val.Value());
                break;
            }
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                sprintf(val_buffer, "%d", (int) val.Value());
                break;
            }
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                sprintf(val_buffer, "%d", (int) val.Value());
                break;
            }
            case eDB_BigInt: {
				// May have problems similar to Test_Procedure2.
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                string s8 = NStr::Int8ToString(val.Value());
                s8.copy(val_buffer, s8.size());
                val_buffer[s8.size()] = '\0';
                break;
            }
            case eDB_Char:
            case eDB_VarChar:
            case eDB_LongChar: {
                CDB_String& val = dynamic_cast<CDB_String&> (param);
                const string& s = val.AsString(); // NB: 255 bytes at most
                string::const_iterator c = s.begin();
                size_t i = 0;
                val_buffer[i++] = '\'';
                while (c != s.end()  &&  i < sizeof(val_buffer) - 2) {
                    if (*c == '\'')
                        val_buffer[i++] = '\'';
                    val_buffer[i++] = *c++;
                }
                if (c != s.end()) return false;
                val_buffer[i++] = '\'';
                val_buffer[i] = '\0';
                break;
            }
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.Size();
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.Size();
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_LongBinary: {
                CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
                const unsigned char* c = (const unsigned char*) val.Value();
                size_t i = 0, size = val.DataSize();
                if(size*2 > sizeof(val_buffer) - 4) return false;
                val_buffer[i++] = '0';
                val_buffer[i++] = 'x';
                for (size_t j = 0; j < size; j++) {
                    val_buffer[i++] = s_hexnum[c[j] >> 4];
                    val_buffer[i++] = s_hexnum[c[j] & 0x0F];
                }
                val_buffer[i++] = '\0';
                break;
            }
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                sprintf(val_buffer, "%E", (double) val.Value());
                break;
            }
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                sprintf(val_buffer, "%E", val.Value());
                break;
            }
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                string t = val.Value().AsString("M/D/Y h:m");
                sprintf(val_buffer, "'%s'", t.c_str());
                break;
            }
            case eDB_DateTime: {
                CDB_DateTime& val =
                    dynamic_cast<CDB_DateTime&> (param);
                string t = val.Value().AsString("M/D/Y h:m:s");
                sprintf(val_buffer, "'%s:%.3d'", t.c_str(),
            (int)(val.Value().NanoSecond()/1000000));
                break;
            }
            default:
                return false;
            }
        } else
            strcpy(val_buffer, "NULL");

        // substitute the param
        m_CombinedQuery = impl::g_SubstituteParam(m_CombinedQuery, name, val_buffer);
    }

    return true;
}


#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif

END_NCBI_SCOPE


